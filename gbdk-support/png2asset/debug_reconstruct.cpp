#include <vector>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "lodepng.h"
#include "png2asset.h"
#include "tiles.h"
#include "metasprites.h"
#include "png_image.h"
#include "process_arguments.h"

using namespace std;

// Reconstruct a single metasprite frame as a PNG for debugging
void ReconstructMetaspriteFrame(PNG2AssetData* assetData, int frame_idx, 
                                int frame_width, int frame_height,
                                const string& output_filename) {
    
    if (frame_idx >= (int)assetData->sprites.size()) {
        printf("Error: Frame %d does not exist (only %d frames)\n", 
               frame_idx, (int)assetData->sprites.size());
        return;
    }

    // Create output image buffer (RGBA)
    int img_w = frame_width;
    int img_h = frame_height;
    vector<unsigned char> image_data(img_w * img_h * 4, 0);
    
    // Fill with transparent white
    for (int i = 0; i < img_w * img_h; ++i) {
        image_data[i * 4 + 0] = 255; // R
        image_data[i * 4 + 1] = 255; // G
        image_data[i * 4 + 2] = 255; // B
        image_data[i * 4 + 3] = 0;   // A (transparent)
    }
    
    MetaSprite& sprite = assetData->sprites[frame_idx];
    int pivot_x = assetData->args->pivot.x;
    int pivot_y = assetData->args->pivot.y;
    
    int tile_w = assetData->image.tile_w;
    int tile_h = assetData->image.tile_h;
    
    printf("\n=== Reconstructing Frame %d ===\n", frame_idx);
    printf("Frame dimensions: %dx%d\n", frame_width, frame_height);
    printf("Pivot: (%d, %d)\n", pivot_x, pivot_y);
    printf("Tile dimensions: %dx%d\n", tile_w, tile_h);
    printf("Number of tiles in frame: %d\n", (int)sprite.size());
    
    // Track accumulated position (metasprite offsets are relative to previous tile)
    int current_x = pivot_x;
    int current_y = pivot_y;
    
    // Define colors for tile boundaries (excluding yellow, using 25% alpha)
    // Cycle through: cyan, magenta, green, red, blue, orange, purple, lime, pink
    unsigned char box_colors[][3] = {
        {0, 255, 255},   // Cyan
        {255, 0, 255},   // Magenta
        {0, 255, 0},     // Green
        {255, 0, 0},     // Red
        {0, 0, 255},     // Blue
        {255, 128, 0},   // Orange
        {128, 0, 255},   // Purple
        {128, 255, 0},   // Lime
        {255, 128, 255}  // Pink
    };
    int num_colors = 9;
    int color_idx = 0;
    
    // Process each tile in the metasprite
    for (size_t i = 0; i < sprite.size(); ++i) {
        MTTile& mt = sprite[i];
        
        // Decode tile index (accounting for sprite mode)
        int tile_idx = mt.offset_idx;
        if (assetData->args->sprite_mode == SPR_8x16)
            tile_idx /= 2;
        else if (assetData->args->sprite_mode == SPR_16x16_MSX)
            tile_idx /= 4;
        
        if (tile_idx >= (int)assetData->tiles.size()) {
            printf("  Warning: Tile %d references invalid tile index %d (max: %d)\n", 
                   (int)i, tile_idx, (int)assetData->tiles.size() - 1);
            continue;
        }
        
        // Accumulate relative offsets to get absolute position
        current_x += mt.offset_x;
        current_y += mt.offset_y;
        
        int x_pos = current_x;
        int y_pos = current_y;
        
        // Extract flip flags and palette
        bool flip_x = (mt.props >> 6) & 1;
        bool flip_y = (mt.props >> 5) & 1;
        int pal_idx = mt.props & 0xF;
        
        printf("  Tile %2d: idx=%2d pos=(%3d,%3d) flip_x=%d flip_y=%d pal=%d\n",
               (int)i, tile_idx, x_pos, y_pos, flip_x, flip_y, pal_idx);
        
        const Tile& tile = assetData->tiles[tile_idx];
        
        // Draw the tile into the output image
        for (int ty = 0; ty < tile_h; ++ty) {
            for (int tx = 0; tx < tile_w; ++tx) {
                // Apply flipping
                int src_x = flip_x ? (tile_w - 1 - tx) : tx;
                int src_y = flip_y ? (tile_h - 1 - ty) : ty;
                
                unsigned char color_idx = tile.data[src_y * tile_w + src_x];
                
                // Skip transparent pixels
                if (color_idx == 0) continue;
                
                // Calculate destination position
                int dst_x = x_pos + tx;
                int dst_y = y_pos + ty;
                
                // Check bounds
                if (dst_x < 0 || dst_x >= img_w || dst_y < 0 || dst_y >= img_h) {
                    continue;
                }
                
                // Get color from palette
                int palette_offset = pal_idx * assetData->image.colors_per_pal * 4; // RGBA
                int color_offset = palette_offset + (color_idx * 4);
                
                // Clamp palette index to available palettes
                int num_palettes = assetData->image.total_color_count / assetData->image.colors_per_pal;
                if (pal_idx >= num_palettes) {
                    // Use palette 0 if requested palette doesn't exist
                    palette_offset = 0;
                    color_offset = color_idx * 4;
                }
                
                if (color_offset >= (int)assetData->image.total_color_count * 4) {
                    printf("  Warning: Color index out of bounds: pal=%d color=%d (using color 0)\n", pal_idx, color_idx);
                    color_offset = 0;
                }
                
                unsigned char* pal_color = &assetData->image.palette[color_offset];
                
                // Write to output image
                int dst_idx = (dst_y * img_w + dst_x) * 4;
                image_data[dst_idx + 0] = pal_color[0]; // R
                image_data[dst_idx + 1] = pal_color[1]; // G
                image_data[dst_idx + 2] = pal_color[2]; // B
                image_data[dst_idx + 3] = 255;          // A (opaque)
            }
        }
        
        // Draw colored box around the tile (25% transparency)
        unsigned char* box_color = box_colors[color_idx];
        
        // Draw horizontal lines (top and bottom)
        for (int tx = 0; tx < tile_w; ++tx) {
            int dst_x = x_pos + tx;
            
            // Top edge
            int dst_y_top = y_pos;
            if (dst_x >= 0 && dst_x < img_w && dst_y_top >= 0 && dst_y_top < img_h) {
                int dst_idx = (dst_y_top * img_w + dst_x) * 4;
                // Blend 25% with existing pixel (75% original + 25% box color)
                image_data[dst_idx + 0] = (image_data[dst_idx + 0] * 3 + box_color[0]) / 4;
                image_data[dst_idx + 1] = (image_data[dst_idx + 1] * 3 + box_color[1]) / 4;
                image_data[dst_idx + 2] = (image_data[dst_idx + 2] * 3 + box_color[2]) / 4;
                image_data[dst_idx + 3] = 255; // Opaque
            }
            
            // Bottom edge
            int dst_y_bottom = y_pos + tile_h - 1;
            if (dst_x >= 0 && dst_x < img_w && dst_y_bottom >= 0 && dst_y_bottom < img_h) {
                int dst_idx = (dst_y_bottom * img_w + dst_x) * 4;
                // Blend 25% with existing pixel (75% original + 25% box color)
                image_data[dst_idx + 0] = (image_data[dst_idx + 0] * 3 + box_color[0]) / 4;
                image_data[dst_idx + 1] = (image_data[dst_idx + 1] * 3 + box_color[1]) / 4;
                image_data[dst_idx + 2] = (image_data[dst_idx + 2] * 3 + box_color[2]) / 4;
                image_data[dst_idx + 3] = 255; // Opaque
            }
        }
        
        // Draw vertical lines (left and right)
        for (int ty = 0; ty < tile_h; ++ty) {
            int dst_y = y_pos + ty;
            
            // Left edge
            int dst_x_left = x_pos;
            if (dst_x_left >= 0 && dst_x_left < img_w && dst_y >= 0 && dst_y < img_h) {
                int dst_idx = (dst_y * img_w + dst_x_left) * 4;
                // Blend 25% with existing pixel (75% original + 25% box color)
                image_data[dst_idx + 0] = (image_data[dst_idx + 0] * 3 + box_color[0]) / 4;
                image_data[dst_idx + 1] = (image_data[dst_idx + 1] * 3 + box_color[1]) / 4;
                image_data[dst_idx + 2] = (image_data[dst_idx + 2] * 3 + box_color[2]) / 4;
                image_data[dst_idx + 3] = 255; // Opaque
            }
            
            // Right edge
            int dst_x_right = x_pos + tile_w - 1;
            if (dst_x_right >= 0 && dst_x_right < img_w && dst_y >= 0 && dst_y < img_h) {
                int dst_idx = (dst_y * img_w + dst_x_right) * 4;
                // Blend 25% with existing pixel (75% original + 25% box color)
                image_data[dst_idx + 0] = (image_data[dst_idx + 0] * 3 + box_color[0]) / 4;
                image_data[dst_idx + 1] = (image_data[dst_idx + 1] * 3 + box_color[1]) / 4;
                image_data[dst_idx + 2] = (image_data[dst_idx + 2] * 3 + box_color[2]) / 4;
                image_data[dst_idx + 3] = 255; // Opaque
            }
        }
        
        // Cycle to next color
        color_idx = (color_idx + 1) % num_colors;
    }
    
    // Save to PNG
    unsigned error = lodepng::encode(output_filename, image_data, img_w, img_h);
    if (error) {
        printf("Error encoding PNG %s: %s\n", output_filename.c_str(), lodepng_error_text(error));
    } else {
        printf("Saved reconstructed frame to: %s\n", output_filename.c_str());
    }
}

// Reconstruct all metasprite frames
void ReconstructAllMetaspriteFrames(PNG2AssetData* assetData, const string& output_prefix) {
    
    if (!assetData->args->export_as_map && assetData->sprites.size() > 0) {
        printf("\n=== METASPRITE RECONSTRUCTION DEBUG ===\n");
        printf("Total frames: %d\n", (int)assetData->sprites.size());
        printf("Sprite dimensions: %dx%d\n", 
               assetData->args->spriteSize.width, 
               assetData->args->spriteSize.height);
        
        for (size_t i = 0; i < assetData->sprites.size(); ++i) {
            char filename[256];
            snprintf(filename, sizeof(filename), "%s_frame%02d_reconstructed.png", 
                    output_prefix.c_str(), (int)i);
            
            ReconstructMetaspriteFrame(assetData, i, 
                                      assetData->args->spriteSize.width,
                                      assetData->args->spriteSize.height,
                                      filename);
        }
        printf("\n=== END RECONSTRUCTION ===\n\n");
    }
}
