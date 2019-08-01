#pragma once
#define NOMINMAX

#include <d3d12.h>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#include "RootSignatureManager.h"

namespace Engine {
	enum ShadingRecordType {
		RayGeneration = 0,
		Miss = 1,
		HitGroup = 2
	};

	struct ShadingRecord {
		std::wstring programName;
		std::string rootSignatureName;
		ShadingRecordType shadingRecordType;

		// paramter values
		std::unordered_map<std::string, UINT32> constantsMap;
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12Resource>> viewsMap;
		std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> descriptorHeapMap;
	};

	struct ShadingTableLayout {
		std::size_t offsetInBytes;
		std::size_t alignedRecordSize;
		std::size_t alignedRecordCollectionSize; // For table
		std::size_t numOfRecords;
	};

	// The shading table will map types (from root signature) to buffers
	class ShadingTable
	{
	public:

		// Keep reference for root signatures
		ShadingTable(std::shared_ptr<RootSignatureManager> rootSignatureManager);
		
		// 
		void addProgram(const std::wstring& programName, ShadingRecordType shadingRecordType, const std::string& rootSignatureName);

		void setInputForDescriptorTableParameter(const std::wstring& programName, const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap);
		void setInputForViewParameter(const std::wstring& programName, const std::string& parameterName, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
		void setInputForConstantParameter(const std::wstring& programName, const std::string& parameterName, UINT32 constant);

		Microsoft::WRL::ComPtr<ID3D12Resource> generateShadingTable(
			Microsoft::WRL::ComPtr<ID3D12Device5> pDevice,
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCurrentCommandList,
			Microsoft::WRL::ComPtr<ID3D12StateObject> pStateObject,
			Microsoft::WRL::ComPtr<ID3D12Resource>& shadingTableTempResource);

		void addProgramAssociationsToSubobject(CD3DX12_STATE_OBJECT_DESC& stateObjectDesc);

		D3D12_DISPATCH_RAYS_DESC getDispatchRaysDescriptor(UINT32 width, UINT32 height) const;

	private:
		void validateInputs();
		void sortShadingRecords();
		size_t getShadingRecordSize(const std::wstring& programName);
		size_t getLargestRecordTypeSize(ShadingRecordType shadingRecordType);

		std::shared_ptr<RootSignatureManager> rootSignatureManager;
		std::unordered_map<std::wstring, size_t> shadingRecordsMap;
		std::vector<ShadingRecord> shadingRecords;

		ShadingTableLayout tableLayout[3];

		std::vector< CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT> vecAssociations;

		Microsoft::WRL::ComPtr<ID3D12Resource> pShadingTable;
	};
}
