#include "RootSignatureManager.h"

#include <algorithm>
#include <stdexcept>

#include "Util/DXUtil.h"

using namespace std;
using namespace Engine;
using namespace Util;

void Engine::RootSignatureManager::addDescriptorRange(const string& destRangeName, const D3D12_DESCRIPTOR_RANGE1& range)
{
	descriptorRanges[destRangeName].push_back(range);
}

void Engine::RootSignatureManager::addDescriptorRange(const string& destRangeName, const std::string& sourceRangeName)
{
	auto& vec = descriptorRanges[destRangeName];
	auto& vec2 = descriptorRanges.at(sourceRangeName);
	vec.insert(vec.end(), vec2.begin(), vec2.end());
}

void Engine::RootSignatureManager::setDescriptorTableParameter(const std::string& destParameterName, const std::string& descriptorRangeName)
{
	CD3DX12_ROOT_PARAMETER1 rootParameter;
	auto& vec = descriptorRanges.at(descriptorRangeName);
	rootParameter.InitAsDescriptorTable(static_cast<UINT>(vec.size()), vec.data());
	setParameter(destParameterName, rootParameter);
}

void Engine::RootSignatureManager::setParameter(const std::string& destParameterName, const D3D12_ROOT_PARAMETER1& parameter)
{
	parameters[destParameterName] = parameter;
}

void Engine::RootSignatureManager::addRootSignature(const std::string& rootSignatureName)
{
	rootSignatures[rootSignatureName] = RootSignature();
}

void Engine::RootSignatureManager::setSamplerForRootSignature(const std::string& rootSignatureName, const D3D12_STATIC_SAMPLER_DESC& sampler)
{
	auto& rs = rootSignatures[rootSignatureName];
	rs.sampler = sampler;
	rs.samplerSet = true;
}

void Engine::RootSignatureManager::addParameterToRootSignature(const std::string& destSignatureName, const std::string& parameterName)
{
	rootSignatures[destSignatureName].parameterNames.push_back(parameterName);
}

void Engine::RootSignatureManager::addParametersToRootSignature(const std::string& destSignatureName, const std::vector<std::string>& parameterNames)
{
	auto& v = rootSignatures[destSignatureName].parameterNames;
	v.insert(v.end(), parameterNames.begin(), parameterNames.end());
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> Engine::RootSignatureManager::generateRootSignature(const string& rootSigatureName, Microsoft::WRL::ComPtr<ID3D12Device5> pDevice)
{
	auto& customRootDesc = rootSignatures.at(rootSigatureName);

	// Collect parameters together
	vector<D3D12_ROOT_PARAMETER1> localParamters;
	for (auto& param : customRootDesc.parameterNames) {
		localParamters.push_back(parameters.at(param));
	}

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(
		static_cast<UINT32>(localParamters.size()),
		localParamters.data(),
		customRootDesc.samplerSet ? 1 : 0,
		customRootDesc.samplerSet ? &customRootDesc.sampler : nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

	return customRootDesc.compiledSignature = DXUtil::createRootSignature(pDevice, rootSignatureDesc);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Engine::RootSignatureManager::generateDescriptorHeapForRangeParameter(const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12Device5> pDevice) const
{
	const auto& parameter = parameters.at(parameterName);

	if (parameter.ParameterType != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		throw runtime_error("Paramter is not of type D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE");

	UINT32 numDescriptors = 0;
	for (UINT32 i = 0; i < parameter.DescriptorTable.NumDescriptorRanges; ++i) {
		const auto& range = parameter.DescriptorTable.pDescriptorRanges[i];
		if (range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SRV && range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_UAV && range.RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
			throw runtime_error("Range Type must be CBV/SRV/UAV - create descriptor heap manually for other types");
		numDescriptors += range.NumDescriptors;
	}

	if (numDescriptors == 0)
		throw runtime_error("Paramter ranges are empty (0 descriptors in each)");

	return DXUtil::createDescriptorHeap(pDevice, numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
}

void Engine::RootSignatureManager::addRootSignaturesToSubObject(CD3DX12_STATE_OBJECT_DESC& stateObjectDesc)
{
	for (auto& rootSignature : rootSignatures) {
		rootSignature.second.subobject.AddToStateObject(stateObjectDesc);
		rootSignature.second.subobject.SetRootSignature(rootSignature.second.compiledSignature.Get());
	}
}

const std::vector<D3D12_DESCRIPTOR_RANGE1>& Engine::RootSignatureManager::getDescriptorRanges(const std::string& descriptorRangeName) const
{
	return descriptorRanges.at(descriptorRangeName);
}

const D3D12_ROOT_PARAMETER1& Engine::RootSignatureManager::getParameter(const std::string& parameterName) const
{
	return parameters.at(parameterName);
}

const D3D12_ROOT_PARAMETER1& Engine::RootSignatureManager::getParameterForRootSignature(const std::string& rootSignatureName, const std::string& parameterName) const
{
	const auto& rs = getRootSignature(rootSignatureName);
	// Find paramter name in root signature
	if (std::find(rs.parameterNames.begin(), rs.parameterNames.end(), parameterName) == rs.parameterNames.end())
		throw runtime_error("No such parmater '" + parameterName + "' in root signature '" + rootSignatureName + "'");

	return parameters.at(parameterName);
}

const RootSignature& Engine::RootSignatureManager::getRootSignature(const std::string& rootSignatureName) const
{
	return rootSignatures.at(rootSignatureName);
}
