#pragma once
#include <string>

class PNG2AssetData;

// Reconstruct metasprite frames as PNG images for debugging
void ReconstructMetaspriteFrame(PNG2AssetData* assetData, int frame_idx,
                                int frame_width, int frame_height,
                                const std::string& output_filename);

void ReconstructAllMetaspriteFrames(PNG2AssetData* assetData, const std::string& output_prefix);
