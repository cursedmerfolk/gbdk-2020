#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <set>
#include <cstdint>

#include "metasprites_flexible.h"
#include "metasprites.h"
#include "tiles.h"
#include "png2asset.h"
#include "image_utils.h"
#include "process_arguments.h"
#include "png_image.h"

using namespace std;

// Helper function to compare tile and image pixels at specific data indices
// Returns true if pixels match, false otherwise
inline bool ComparePixels(const Tile& tile, const PNGImage& image,
                          int tile_data_idx, int image_data_idx) {
    unsigned char tile_color_idx = tile.data[tile_data_idx];
    unsigned char image_color_global_idx = image.data[image_data_idx];
    
    // Get RGB values to compare
    // Tile uses palette number + color index within palette
    // Image uses global color index into combined palette
    // Each palette entry is 4 bytes (RGBA)
    int tile_palette_offset = (tile.pal * image.colors_per_pal + tile_color_idx) * 4;
    int image_palette_offset = image_color_global_idx * 4;
    
    unsigned char tile_r = image.palette[tile_palette_offset + 0];
    unsigned char tile_g = image.palette[tile_palette_offset + 1];
    unsigned char tile_b = image.palette[tile_palette_offset + 2];
    unsigned char tile_a = image.palette[tile_palette_offset + 3];
    
    unsigned char image_r = image.palette[image_palette_offset + 0];
    unsigned char image_g = image.palette[image_palette_offset + 1];
    unsigned char image_b = image.palette[image_palette_offset + 2];
    unsigned char image_a = image.palette[image_palette_offset + 3];
    
    // Check transparency based on alpha channel
    bool tile_transparent = (tile_a == 0);
    bool image_transparent = (image_a == 0);
    
    // Transparent tile pixels match anything
    if (tile_transparent) {
        return true;
    }
    // Non-transparent tile pixel cannot match transparent image pixel
    if (image_transparent) {
        return false;
    }
    
    // Compare R, G, B
    return (tile_r == image_r && tile_g == image_g && tile_b == image_b);
}

// Check if a tile matches at a specific pixel position in the image
// Compares pixel-by-pixel, ignoring transparent pixels (color index 0)
// Takes into account that tile and image may use different palettes
bool TileMatchesAtPosition(const Tile& tile, const PNGImage& image,
                           int pos_x, int pos_y, 
                           int tile_w, int tile_h, int threshold_pixels) {
    // Check bounds
    if (pos_x + tile_w > (int)image.w || pos_y + tile_h > (int)image.h) {
        return false;
    }
    if (pos_x < 0 || pos_y < 0) {
        return false;
    }

    // Compare each pixel in the tile
    int matched_pixels = 0;
    
    for (int y = 0; y < tile_h; ++y) {
        for (int x = 0; x < tile_w; ++x) {
            int tile_data_idx = y * tile_w + x;
            int image_data_idx = image.w * (pos_y + y) + (pos_x + x);
            
            if (ComparePixels(tile, image, tile_data_idx, image_data_idx)) {
                matched_pixels++;
            }
        }
    }
    
    // Check if matched pixels meets threshold
    return matched_pixels >= threshold_pixels;
}

// Check if a horizontally flipped tile matches at a position
bool TileMatchesAtPositionFlipH(const Tile& tile, const PNGImage& image, int pos_x, int pos_y, 
                                int tile_w, int tile_h, int threshold_pixels) {
    if (pos_x + tile_w > (int)image.w || pos_y + tile_h > (int)image.h) {
        return false;
    }
    if (pos_x < 0 || pos_y < 0) {
        return false;
    }

    int matched_pixels = 0;
    
    for (int y = 0; y < tile_h; ++y) {
        for (int x = 0; x < tile_w; ++x) {
            // Flip horizontally: reverse x coordinate
            int tile_data_idx = y * tile_w + (tile_w - 1 - x);
            int image_data_idx = image.w * (pos_y + y) + (pos_x + x);
            
            if (ComparePixels(tile, image, tile_data_idx, image_data_idx)) {
                matched_pixels++;
            }
        }
    }
    
    return matched_pixels >= threshold_pixels;
}

// Check if a vertically flipped tile matches at a position
bool TileMatchesAtPositionFlipV(const Tile& tile, const PNGImage& image, int pos_x, int pos_y, 
                                int tile_w, int tile_h, int threshold_pixels) {
    if (pos_x + tile_w > (int)image.w || pos_y + tile_h > (int)image.h) {
        return false;
    }
    if (pos_x < 0 || pos_y < 0) {
        return false;
    }

    int matched_pixels = 0;
    
    for (int y = 0; y < tile_h; ++y) {
        for (int x = 0; x < tile_w; ++x) {
            // Flip vertically: reverse y coordinate
            int tile_data_idx = (tile_h - 1 - y) * tile_w + x;
            int image_data_idx = image.w * (pos_y + y) + (pos_x + x);
            
            if (ComparePixels(tile, image, tile_data_idx, image_data_idx)) {
                matched_pixels++;
            }
        }
    }
    
    return matched_pixels >= threshold_pixels;
}

// Check if a tile flipped both ways matches at a position
bool TileMatchesAtPositionFlipHV(const Tile& tile, const PNGImage& image, int pos_x, int pos_y, 
                                 int tile_w, int tile_h, int threshold_pixels) {
    if (pos_x + tile_w > (int)image.w || pos_y + tile_h > (int)image.h) {
        return false;
    }
    if (pos_x < 0 || pos_y < 0) {
        return false;
    }

    int matched_pixels = 0;
    
    for (int y = 0; y < tile_h; ++y) {
        for (int x = 0; x < tile_w; ++x) {
            // Flip both: reverse both coordinates
            int tile_data_idx = (tile_h - 1 - y) * tile_w + (tile_w - 1 - x);
            int image_data_idx = image.w * (pos_y + y) + (pos_x + x);
            
            if (ComparePixels(tile, image, tile_data_idx, image_data_idx)) {
                matched_pixels++;
            }
        }
    }
    
    return matched_pixels >= threshold_pixels;
}

// Find all positions where tiles from the tileset match in a sprite frame area
// This scans every pixel position, not just grid-aligned positions
void FindFlexibleTileMatches(PNG2AssetData* assetData,
                             int frame_x, int frame_y, 
                             int frame_w, int frame_h, 
                             vector<TileMatch>& matches) {
    matches.clear();
    
    int tile_w = assetData->image.tile_w;
    int tile_h = assetData->image.tile_h;
    
    // Debug: print palette info for first few source tiles (only once)
    static bool debug_printed = false;
    if (!debug_printed) {
        printf("\n=== Source Tile Palette Debug ===\n");
        fflush(stdout);
        for (size_t i = 0; i < assetData->args->source_tileset_size; ++i) {
            printf("Tile %2zu: pal=%d\n", i, assetData->tiles[i].pal);
        }
        fflush(stdout);
        debug_printed = true;
    }

    // For each tile in the source tileset
    for (size_t tile_idx = 0; tile_idx < assetData->args->source_tileset_size; ++tile_idx) {
        const Tile& tile = assetData->tiles[tile_idx];
        
        // Debug: print palette for first few tiles
        if (assetData->args->debug_reconstruct && tile_idx < 10) {
            printf("Source tile %zu has palette index: %d\n", tile_idx, tile.pal);
        }
        
        // Scan every pixel position in the frame
        for (int y = frame_y; y <= frame_y + frame_h - tile_h; ++y) {
            for (int x = frame_x; x <= frame_x + frame_w - tile_w; ++x) {
                unsigned char props = assetData->args->props_default;
                bool matched = false;
                
                int threshold_pixels = assetData->args->match_threshold_pixels;
                
                // Try normal orientation
                if (TileMatchesAtPosition(tile, assetData->image, x, y, tile_w, tile_h, threshold_pixels)) {
                    matched = true;
                    props = assetData->args->props_default;
                }
                // Try flipped versions if enabled
                else if (assetData->args->flip_tiles) {
                    if (TileMatchesAtPositionFlipV(tile, assetData->image, x, y, tile_w, tile_h, threshold_pixels)) {
                        matched = true;
                        props = assetData->args->props_default | (1 << 5); // VFLIP
                    }
                    else if (TileMatchesAtPositionFlipHV(tile, assetData->image, x, y, tile_w, tile_h, threshold_pixels)) {
                        matched = true;
                        props = assetData->args->props_default | (1 << 5) | (1 << 6); // VFLIP | HFLIP
                    }
                    else if (TileMatchesAtPositionFlipH(tile, assetData->image, x, y, tile_w, tile_h, threshold_pixels)) {
                        matched = true;
                        props = assetData->args->props_default | (1 << 6); // HFLIP
                    }
                }
                
                if (matched) {
                    // Use the palette index stored in the tile from the source tileset
                    unsigned char pal_idx = tile.pal;
                    props |= pal_idx;
                    matches.push_back(TileMatch(tile_idx, x, y, props));
                }
            }
        }
    }
}

// Generate metasprite using flexible tile matching
void GetMetaSpriteFlexible(int _x, int _y,
                           int _w, int _h,
                           int pivot_x,int pivot_y, 
                           PNG2AssetData* assetData) {
    static bool first_call = true;
    if (first_call) {
        printf("DEBUG: GetMetaSpriteFlexible called for first time\n");
        fflush(stdout);
        first_call = false;
    }
    
    vector<TileMatch> matches;
    FindFlexibleTileMatches(assetData, _x, _y, _w, _h, matches);
    
    assetData->sprites.push_back(MetaSprite());
    MetaSprite& mt_sprite = assetData->sprites.back();
    
    // Track last position for relative offsets (standard metasprite format)
    int last_x = _x + pivot_x;
    int last_y = _y + pivot_y;
    
    // Convert matches to MTTiles
    for (const TileMatch& match : matches) {
        size_t idx = match.tile_idx;
        
        // Scale up index based on 8x8 tiles-per-hardware sprite
        if(assetData->args->sprite_mode == SPR_8x16)
            idx *= 2;
        else if(assetData->args->sprite_mode == SPR_16x16_MSX)
            idx *= 4;
        
        // Calculate relative offset from last tile position (standard metasprite format)
        int offset_x = match.x - last_x;
        int offset_y = match.y - last_y;
        
        mt_sprite.push_back(MTTile(offset_x, offset_y, (unsigned char)idx, match.props));
        
        // Update last position for next tile
        last_x = match.x;
        last_y = match.y;
    }
    
    printf("Generated metasprite frame at (%d,%d) with %d tiles using flexible matching [NEW CODE]\n", 
           _x, _y, (int)matches.size());
}
