#include "MipMapContainer.h"

void Plugin::MipMapContainer::AddMipMap(const std::string& name, DirectX::ScratchImage image) {
	std::unique_lock<std::shared_mutex> lock(mutex_);
	mipMaps_.emplace(name, std::move(image));
}

size_t Plugin::MipMapContainer::GetMipMapCount() const {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	return mipMaps_.size();
}

const DirectX::ScratchImage* Plugin::MipMapContainer::GetMipMap(const std::string& name) const {
	std::shared_lock<std::shared_mutex> lock(mutex_);
	auto it = mipMaps_.find(name);
	if (it != mipMaps_.end()) {
		return &it->second;
	}
	return nullptr;
}
