#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : mSize{size} {}

    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        constexpr size_t kFail = std::numeric_limits<size_t>::max();
        float x = mX, y = mLineY, lineH = mLineHeight;

        if (x + rectSize.x > mSize.x) {  // doesn't fit on this line -> start a new one
            y += lineH;
            x = 0.f;
            lineH = 0.f;
        }
        if (x + rectSize.x > mSize.x || y + rectSize.y > mSize.y) {
            return {false, kFail};       // too wide for the bin, or no vertical room
        }
        mRects.push_back(sf::FloatRect({x, y}, rectSize));
        mX = x + rectSize.x;
        mLineY = y;
        mLineHeight = std::max(lineH, rectSize.y);
        return {true, mRects.size() - 1};
    }

    sf::FloatRect getBounds(size_t rectID) const { return mRects.at(rectID); }

private:
    sf::Vector2f mSize;
    float mLineY = 0.f, mX = 0.f, mLineHeight = 0.f;
    std::vector<sf::FloatRect> mRects;
};

// Returns a new image which pads the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, unsigned int pixels) {
    const int w = static_cast<int>(img.getSize().x);
    const int h = static_cast<int>(img.getSize().y);
    const int p = static_cast<int>(pixels);
    sf::Image out(sf::Vector2u(w + 2 * p, h + 2 * p), sf::Color::Transparent);
    if (w == 0 || h == 0) return out;
    for (int y = 0; y < h + 2 * p; ++y) {
        for (int x = 0; x < w + 2 * p; ++x) {
            int sx = std::clamp(x - p, 0, w - 1);
            int sy = std::clamp(y - p, 0, h - 1);
            out.setPixel({(unsigned)x, (unsigned)y}, img.getPixel({(unsigned)sx, (unsigned)sy}));
        }
    }
    return out;
}

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {
        mTexture.update(sf::Image(binSize, sf::Color::Transparent));
    }

    const sf::Texture& getTexture() const { return mTexture; }

    TextureID registerTexture(const std::string& filepath) {
        sf::Image img;
        if (!img.loadFromFile(filepath)) throw std::runtime_error("Failed to load " + filepath);
        sf::Image padded = padImage(img, static_cast<unsigned int>(mPadding));
        auto [ok, id] = mPacker.pack(sf::Vector2f(padded.getSize()));
        if (!ok) throw std::runtime_error("Atlas is full");
        mTexture.update(padded, sf::Vector2u(mPacker.getBounds(id).position));
        return id;
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        sf::FloatRect b = mPacker.getBounds(id);
        return {static_cast<unsigned>(b.size.x) - 2 * static_cast<unsigned>(mPadding),
                static_cast<unsigned>(b.size.y) - 2 * static_cast<unsigned>(mPadding)};
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        sf::FloatRect b = mPacker.getBounds(id);
        const float pad = static_cast<float>(mPadding);
        const sf::Vector2f atlas(mTexture.getSize());
        // unpadded rect in image coords (origin top-left, y down)
        float left = b.position.x + pad, top = b.position.y + pad;
        float w = b.size.x - 2 * pad, h = b.size.y - 2 * pad;
        // convert to UV (origin bottom-left, y up): the rect's *bottom* edge gives the min v
        float u = left / atlas.x;
        float v = 1.f - (top + h) / atlas.y;
        return sf::FloatRect({u, v}, {w / atlas.x, h / atlas.y});
    }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
