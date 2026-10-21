#pragma once
#include <DirectXTex.h>
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <functional>

namespace Plugin {
	/// @brief テクスチャのミップマップを管理するクラス
	class MipMapContainer final {
	public:
		/// @brief コンストラクタ
		MipMapContainer() = default;
		/// @brief デストラクタ
		~MipMapContainer() = default;
		// コピーとムーブを禁止
		MipMapContainer(const MipMapContainer&) = delete;
		MipMapContainer& operator=(const MipMapContainer&) = delete;
		MipMapContainer(MipMapContainer&&) = delete;
		MipMapContainer& operator=(MipMapContainer&&) = delete;

		/// @brief ミップマップを追加する
		/// @param name ミップマップの名前
		/// @param image ミップマップの画像データ
		void AddMipMap(const std::string& name, DirectX::ScratchImage image);

		/// @brief ミップマップの数を返す
		size_t GetMipMapCount() const;
		/// @brief ミップマップを取得する
		/// @param name ミップマップの名前
		const DirectX::ScratchImage* GetMipMap(const std::string& name) const;

	private:
		std::unordered_map<std::string, DirectX::ScratchImage> mipMaps_;
		mutable std::shared_mutex mutex_;
	};
}