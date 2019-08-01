#include "ShadingTable.h"

#include "Util/DXUtil.h"
#include <stdexcept>
#include <algorithm>

using namespace std;
using namespace Engine;
using namespace Util;

Engine::ShadingTable::ShadingTable(std::shared_ptr<RootSignatureManager> rootSignatureManager)
	: rootSignatureManager (rootSignatureManager)//, numHitGroupGeometries()
{}

void Engine::ShadingTable::addProgram(const wstring& programName, ShadingRecordType shadingRecordType, const string& rootSignatureName)
{
	shadingRecordsMap[programName] = shadingRecords.size();
	shadingRecords.push_back({ programName, rootSignatureName, shadingRecordType });
}

void Engine::ShadingTable::setInputForDescriptorTableParameter(const wstring& programName, const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	// Get Program data
	auto& programStruct = shadingRecords[shadingRecordsMap[programName]];

	// Check if correct type
	const auto& parameter = rootSignatureManager->getParameterForRootSignature(programStruct.rootSignatureName, parameterName);
	if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		throw runtime_error("Paramter '" + parameterName + "' not a descriptor table type");

	// Set
	programStruct.descriptorHeapMap[parameterName] = descriptorHeap;
}

void Engine::ShadingTable::setInputForViewParameter(const wstring& programName, const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
	// Get Program data
	auto& programStruct = shadingRecords[shadingRecordsMap[programName]];

	// Check if correct type
	const auto& parameter = rootSignatureManager->getParameterForRootSignature(programStruct.rootSignatureName, parameterName);
	if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_CBV && parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_SRV && parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_UAV)
		throw runtime_error("Paramter '" + parameterName + "' not a CBV/SRV/UAV type");
	
	// Set
	programStruct.viewsMap[parameterName] = resource;
}

void Engine::ShadingTable::setInputForConstantParameter(const wstring& programName, const std::string& parameterName, UINT32 constant)
{
	// Get Program data
	auto& programStruct = shadingRecords[shadingRecordsMap[programName]];

	// Check if correct type
	const auto& parameter = rootSignatureManager->getParameterForRootSignature(programStruct.rootSignatureName, parameterName);
	if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
		throw runtime_error("Paramter '" + parameterName + "' not a 32bit constant type");

	// Set
	programStruct.constantsMap[parameterName] = constant;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Engine::ShadingTable::generateShadingTable(
	Microsoft::WRL::ComPtr<ID3D12Device5> pDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCurrentCommandList,
	Microsoft::WRL::ComPtr<ID3D12StateObject> pStateObject,
	Microsoft::WRL::ComPtr<ID3D12Resource> shadingTableTempResource)
{
	validateInputs();
	sortShadingRecords();

	// Calculate size per record type and total table size
	size_t totalTableSize = 0;
	for (size_t i = 0; i < std::size(tableLayout); ++i) {
		tableLayout[i].offsetInBytes = totalTableSize;

		// Assuming sorted shading records, find respective amounts
		size_t typeCount = 0;
		for (const auto& shadingRecord : shadingRecords) {
			if (shadingRecord.shadingRecordType == static_cast<ShadingRecordType>(i)) {
				++typeCount;
			}
			else if (typeCount > 0) {
				break;
			}
		}

		tableLayout[i].numOfRecords = typeCount;
		tableLayout[i].alignedRecordSize = getLargestRecordTypeSize(static_cast<ShadingRecordType>(i));

		// aligned record size
		auto t = numeric_limits<size_t>::max();
		std::align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, tableLayout[i].alignedRecordSize, (void*&)tableLayout[i].alignedRecordSize, t);

		// Find table alignment 
		tableLayout[i].alignedRecordCollectionSize = tableLayout[i].numOfRecords * tableLayout[i].alignedRecordSize;
		t = numeric_limits<size_t>::max();
		std::align(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, tableLayout[i].alignedRecordCollectionSize, (void*&)tableLayout[i].alignedRecordCollectionSize, t);

		totalTableSize += tableLayout[i].alignedRecordCollectionSize;
	}

	// Create the shading table
	// Create host side buffer - it should be zero initialised when using make_unique..
	unique_ptr<uint8_t[]> bufferManager = make_unique<uint8_t[]>(totalTableSize);
	uint8_t* const buffer = bufferManager.get();
	uint8_t* localBuffer = buffer;

	// Extract the properties interface
	Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> pStateObjectProps;
	pStateObject.As(&pStateObjectProps);
	
	ShadingRecordType shadingRecordType = RayGeneration;
	size_t localRecordNumber = 0;
	// Write each shading record in the buffer
	for (const auto& shadingRecord : shadingRecords) {

		if (shadingRecordType != shadingRecord.shadingRecordType) {
			shadingRecordType = shadingRecord.shadingRecordType;
			localBuffer = buffer + tableLayout[shadingRecordType].offsetInBytes;
			localRecordNumber = 0;
		}

		memcpy(localBuffer, pStateObjectProps->GetShaderIdentifier(shadingRecord.programName.c_str()), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		localBuffer += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

		// Get parameters
		const auto& rs = rootSignatureManager->getRootSignature(shadingRecord.rootSignatureName);
		for (const auto& parameterName : rs.parameterNames) {
			const auto& parameter = rootSignatureManager->getParameterForRootSignature(shadingRecord.rootSignatureName, parameterName);

			switch (parameter.ParameterType) {
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				{	// Get heap
					const auto& descriptorHeap = shadingRecord.descriptorHeapMap.at(parameterName);
					UINT64 descriptorTableHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
					memcpy(localBuffer, &descriptorTableHandle, sizeof(descriptorTableHandle));
				}
				break;
				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				{
					throw runtime_error("D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS not implemented in ShadingTable");
				}
				break;
				case D3D12_ROOT_PARAMETER_TYPE_CBV:
				case D3D12_ROOT_PARAMETER_TYPE_SRV:
				case D3D12_ROOT_PARAMETER_TYPE_UAV:
				{
					const auto& resource = shadingRecord.viewsMap.at(parameterName);
					auto handle = resource->GetGPUVirtualAddress();
					memcpy(localBuffer, &handle, sizeof(handle));
				}
				break;
			}

			localBuffer += 8;
		}

		// Calculate next record location
		localBuffer = buffer + tableLayout[shadingRecord.shadingRecordType].alignedRecordSize * ++localRecordNumber;
	}

	// Upload buffer to gpu
	return DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, shadingTableTempResource, buffer, totalTableSize, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void Engine::ShadingTable::addProgramAssociationsToSubobject(CD3DX12_STATE_OBJECT_DESC& stateObjectDesc)
{
	// build map
	unordered_map<string, vector<wstring>> associations;
	
	for (const auto& shadingRecord : shadingRecords) {
		associations[shadingRecord.rootSignatureName].push_back(shadingRecord.programName);
	}

	vecAssociations.clear();
	vecAssociations.reserve(associations.size()); // DO NOT REMOVE THIS LINE
	for (auto& association : associations) {
		vecAssociations.emplace_back(stateObjectDesc);

		CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT& rgAssociation = vecAssociations[vecAssociations.size() - 1];
		for (auto& programName : association.second) {
			rgAssociation.AddExport(programName.c_str());
		}

		const auto& rs = rootSignatureManager->getRootSignature(association.first);
		const auto& subobject = rootSignatureManager->getRootSignature(association.first).subobject;
		rgAssociation.SetSubobjectToAssociate(subobject);
	}
}

void Engine::ShadingTable::validateInputs()
{
	bool rayGenEncountered = false;
	// Check that all parameters are set for every record
	for (const auto& shadingRecord : shadingRecords) {

		if (rayGenEncountered && shadingRecord.shadingRecordType == RayGeneration)
			throw runtime_error("Only one ray gen program is allowed");
		rayGenEncountered = shadingRecord.shadingRecordType == RayGeneration;

		const auto& rs = rootSignatureManager->getRootSignature(shadingRecord.rootSignatureName);
		for (const auto& parameterName : rs.parameterNames) {
			const auto& parameter = rootSignatureManager->getParameterForRootSignature(shadingRecord.rootSignatureName, parameterName);

			switch (parameter.ParameterType) {
				case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
					if (shadingRecord.descriptorHeapMap.find(parameterName) == shadingRecord.descriptorHeapMap.end())
						throw runtime_error("Empty input for parameter '" + parameterName + "'");
					break;
				case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
					if (shadingRecord.constantsMap.find(parameterName) == shadingRecord.constantsMap.end())
						throw runtime_error("Empty input for parameter '" + parameterName + "'");
					break;
				case D3D12_ROOT_PARAMETER_TYPE_CBV:
				case D3D12_ROOT_PARAMETER_TYPE_SRV:
				case D3D12_ROOT_PARAMETER_TYPE_UAV:
					if (shadingRecord.viewsMap.find(parameterName) == shadingRecord.viewsMap.end())
						throw runtime_error("Empty input for parameter '" + parameterName + "'");
					break;
			}
		}
	}
}

void Engine::ShadingTable::sortShadingRecords()
{
	std::stable_sort(shadingRecords.begin(), shadingRecords.end(), [](const ShadingRecord& a, const ShadingRecord& b) -> int {
		return a.shadingRecordType < b.shadingRecordType;
	});

	// Correct the shading record map
	size_t i = 0;
	for (const auto& shadingRecord : shadingRecords) {
		shadingRecordsMap[shadingRecord.programName] = i++;
	}
}

size_t Engine::ShadingTable::getShadingRecordSize(const std::wstring& programName)
{
	const auto& shadingRecord = shadingRecords[shadingRecordsMap[programName]];
	const auto& rs = rootSignatureManager->getRootSignature(shadingRecord.rootSignatureName);
	return D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + rs.parameterNames.size() * sizeof(UINT64);
}

size_t Engine::ShadingTable::getLargestRecordTypeSize(ShadingRecordType shadingRecordType)
{
	size_t recordSize = 0;
	for (const auto& shadingRecord : shadingRecords) {
		if (shadingRecord.shadingRecordType == shadingRecordType) {
			size_t srSize = getShadingRecordSize(shadingRecord.programName);
			recordSize = max(recordSize, srSize);
		}
	}

	return recordSize;
}
