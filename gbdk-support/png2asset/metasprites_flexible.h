#pragma once
#include <vector>
#include "mttile.h"
#include "tiles.h"
#include "png_image.h"

class PNG2AssetData;

using namespace std;

// Structure to hold a tile match position
struct TileMatch {
    size_t tile_idx;        // Index of the tile in the tileset
    int x;                  // X position where tile matches (in pixels, not grid-aligned)
    int y;                  // Y position where tile matches (in pixels, not grid-aligned)
    unsigned char props;    // Tile properties (flip flags, palette)

    TileMatch(size_t idx, int _x, int _y, unsigned char _props)
        : tile_idx(idx), x(_x), y(_y), props(_props) {}
};

// Function to check if a tile matches at a specific pixel position in the image
// Returns true if there's a match (ignoring transparent pixels)
bool TileMatchesAtPosition(const Tile& tile, const PNGImage& image, int pos_x, int pos_y,
                           int tile_w, int tile_h);

// Find all positions where tiles from the tileset match in a sprite frame area
void FindFlexibleTileMatches(PNG2AssetData* assetData, int frame_x, int frame_y,
                             int frame_w, int frame_h,
                             vector<TileMatch>& matches);

// Generate metasprite using flexible tile matching
void GetMetaSpriteFlexible(int _x, int _y, int _w, int _h, int pivot_x, int pivot_y,
                           PNG2AssetData* assetData);
