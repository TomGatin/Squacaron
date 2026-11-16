/**
 * @file ScreenDrawService.hpp
 * @brief Raylib implementation of IScreenDrawService.
 */

#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "render/api/IScreenDrawService.hpp"
#include "render/api/components/Renderable.hpp"
#include "render/font/FontStore.hpp"
#include "render/texture/TextureStore.hpp"

namespace sq::render {

/**
 * @brief Queues screen-space draw calls and draws them with raylib.
 *
 * Owned by `RenderPlugin`, which calls `flush` after the world pass and before ending the frame, so overlays are
 * drawn on top of the world in the frame they were submitted. Owns the fonts it loads; must be destroyed before the
 * window.
 */
class ScreenDrawService final : public sq::engine::IScreenDrawService {
  public:
    /**
     * @brief Creates an empty service.
     * @param textures Store `drawTexture` reads from; it must outlive the service. Null draws no texture.
     */
    explicit ScreenDrawService(const TextureStore* textures = nullptr) noexcept : _textures(textures) {}

    /**
     * @brief Loads a font file.
     * @param path Path to the font file.
     * @return Identifier of the font, or `sq::engine::NO_FONT` on failure.
     */
    sq::engine::FontId loadFont(const std::string& path) override;

    /**
     * @brief Gets the store of the fonts loaded through `loadFont`, for the asset manager's font loader.
     * @return Font store.
     */
    [[nodiscard]] FontStore& fonts() noexcept { return _fonts; }

    /**
     * @brief Measures text with the given font.
     * @param text Text to measure.
     * @param fontSize Font size in pixels.
     * @param spacing Extra space between characters in pixels.
     * @param font Font to use; `sq::engine::NO_FONT` or an unknown id selects the default font.
     * @return Size of the text in pixels.
     */
    [[nodiscard]] sq::engine::TextSize measureText(std::string_view text, float fontSize, float spacing,
                                                   sq::engine::FontId font) const override;

    /**
     * @brief Queues a filled rectangle.
     * @param rect Rectangle in screen pixels.
     * @param color Fill color.
     */
    void fillRect(const sq::engine::ScreenRect& rect, const sq::ecs::Rgba& color) override;

    /**
     * @brief Queues a rectangle outline.
     * @param rect Rectangle in screen pixels.
     * @param thickness Line thickness in pixels.
     * @param color Line color.
     */
    void strokeRect(const sq::engine::ScreenRect& rect, float thickness, const sq::ecs::Rgba& color) override;

    /**
     * @brief Queues a piece of text.
     * @param text Text to draw (copied).
     * @param x Left edge in pixels.
     * @param y Top edge in pixels.
     * @param fontSize Font size in pixels.
     * @param spacing Extra space between characters in pixels.
     * @param color Text color.
     * @param font Font to use; `sq::engine::NO_FONT` or an unknown id selects the default font.
     */
    void drawText(std::string_view text, float x, float y, float fontSize, float spacing, const sq::ecs::Rgba& color,
                  sq::engine::FontId font) override;

    /**
     * @brief Queues a textured quad.
     * @param texture Texture of the store given at construction; an unknown id draws nothing.
     * @param source Region of the texture; a zero-sized region selects the whole texture.
     * @param dest Rectangle in screen pixels.
     * @param rotation Rotation around the center of `dest`, in radians.
     * @param tint Color multiplied with the texture.
     */
    void drawTexture(sq::ecs::TextureId texture, const sq::ecs::TextureRect& source, const sq::engine::ScreenRect& dest,
                     float rotation, const sq::ecs::Rgba& tint) override;

    /**
     * @brief Draws every queued call in submission order, then empties the queue.
     *
     * Must be called between `Window::beginFrame` and `Window::endFrame`.
     */
    void flush();

  private:
    /** @brief Queued filled rectangle. */
    struct FillCommand {
        /** @brief Rectangle in screen pixels. */
        sq::engine::ScreenRect rect;
        /** @brief Fill color. */
        sq::ecs::Rgba color;
    };

    /** @brief Queued rectangle outline. */
    struct StrokeCommand {
        /** @brief Rectangle in screen pixels. */
        sq::engine::ScreenRect rect;
        /** @brief Line thickness in pixels. */
        float thickness = 0.0F;
        /** @brief Line color. */
        sq::ecs::Rgba color;
    };

    /** @brief Queued text. */
    struct TextCommand {
        /** @brief Owned copy of the text. */
        std::string text;
        /** @brief Left edge in pixels. */
        float x = 0.0F;
        /** @brief Top edge in pixels. */
        float y = 0.0F;
        /** @brief Font size in pixels. */
        float fontSize = 0.0F;
        /** @brief Extra space between characters in pixels. */
        float spacing = 0.0F;
        /** @brief Text color. */
        sq::ecs::Rgba color;
        /** @brief Font identifier. */
        sq::engine::FontId font = sq::engine::NO_FONT;
    };

    /** @brief Queued textured quad. */
    struct TextureCommand {
        /** @brief Texture identifier. */
        sq::ecs::TextureId texture = sq::ecs::NO_TEXTURE;
        /** @brief Region of the texture. */
        sq::ecs::TextureRect source;
        /** @brief Rectangle in screen pixels. */
        sq::engine::ScreenRect dest;
        /** @brief Rotation around the center of `dest`, in radians. */
        float rotation = 0.0F;
        /** @brief Tint. */
        sq::ecs::Rgba tint;
    };

    /** @brief Any queued draw call. */
    using Command = std::variant<FillCommand, StrokeCommand, TextCommand, TextureCommand>;

    /**
     * @brief Draws one queued textured quad.
     * @param command The quad.
     */
    void drawQueued(const TextureCommand& command) const;

    /** @brief Fonts loaded through `loadFont`. */
    FontStore _fonts;
    /** @brief Textures `drawTexture` reads from (not owned), or null. */
    const TextureStore* _textures = nullptr;
    /** @brief Draw calls of the current frame; capacity is kept between frames. */
    std::vector<Command> _commands;
};

}  // namespace sq::render
