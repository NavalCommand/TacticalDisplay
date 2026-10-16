#include <NavalCommand/Symbology/Nato/ImGui/Renderer.h>
#include <NavalCommand/TacticalDisplay/SymbolLayout.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <imgui.h>
#include <vector>

namespace NavalCommand::TacticalDisplay
{
	namespace
	{
		constexpr int IMAGE_WIDTH = 1000;
		constexpr int IMAGE_HEIGHT = 650;

		// Test-only CPU rasterization of the real ImGui triangles and font atlas.
		// No GPU, Sea Power, screenshot utility or runtime dependency is needed.
		void SaveImage(const ImDrawList& drawing, const char* filename)
		{
			unsigned char* texture{};
			int textureWidth{};
			int textureHeight{};
			ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&texture, &textureWidth, &textureHeight);
			std::vector<std::array<unsigned char, 3>> pixels(IMAGE_WIDTH * IMAGE_HEIGHT, {13, 30, 41});
			const auto edge = [](ImVec2 first, ImVec2 second, ImVec2 point)
			{
				return (second.x - first.x) * (point.y - first.y) - (second.y - first.y) * (point.x - first.x);
			};
			for (const auto& command : drawing.CmdBuffer)
			{
				if (command.UserCallback)
				{
					continue;
				}
				for (unsigned int offset = command.IdxOffset; offset < command.IdxOffset + command.ElemCount; offset += 3)
				{
					std::array<ImDrawVert, 3> vertices;
					for (unsigned int index = 0; index < 3; ++index)
					{
						vertices[index] = drawing.VtxBuffer[command.VtxOffset + drawing.IdxBuffer[offset + index]];
					}
					const float area = edge(vertices[0].pos, vertices[1].pos, vertices[2].pos);
					if (std::abs(area) < 0.0001f)
					{
						continue;
					}
					const int left = std::max(0, static_cast<int>(std::floor(std::max(command.ClipRect.x, std::min({vertices[0].pos.x, vertices[1].pos.x, vertices[2].pos.x})))));
					const int right =
						std::min(IMAGE_WIDTH - 1, static_cast<int>(std::ceil(std::min(command.ClipRect.z, std::max({vertices[0].pos.x, vertices[1].pos.x, vertices[2].pos.x})))));
					const int top = std::max(0, static_cast<int>(std::floor(std::max(command.ClipRect.y, std::min({vertices[0].pos.y, vertices[1].pos.y, vertices[2].pos.y})))));
					const int bottom =
						std::min(IMAGE_HEIGHT - 1, static_cast<int>(std::ceil(std::min(command.ClipRect.w, std::max({vertices[0].pos.y, vertices[1].pos.y, vertices[2].pos.y})))));
					for (int row = top; row <= bottom; ++row)
					{
						for (int column = left; column <= right; ++column)
						{
							const ImVec2 sample{column + 0.5f, row + 0.5f};
							const std::array weights{edge(vertices[1].pos, vertices[2].pos, sample) / area,
													 edge(vertices[2].pos, vertices[0].pos, sample) / area,
													 edge(vertices[0].pos, vertices[1].pos, sample) / area};
							if (std::ranges::any_of(weights,
													[](float weight)
													{
														return weight < 0.0f;
													}))
							{
								continue;
							}
							ImVec2 uv{};
							std::array<float, 4> color{};
							for (std::size_t index = 0; index < 3; ++index)
							{
								uv.x += vertices[index].uv.x * weights[index];
								uv.y += vertices[index].uv.y * weights[index];
								for (int channel = 0; channel < 4; ++channel)
								{
									color[channel] += ((vertices[index].col >> (channel * 8)) & 255) * weights[index];
								}
							}
							const int textureColumn = std::clamp(static_cast<int>(uv.x * textureWidth), 0, textureWidth - 1);
							const int textureRow = std::clamp(static_cast<int>(uv.y * textureHeight), 0, textureHeight - 1);
							const auto* texel = texture + (textureRow * textureWidth + textureColumn) * 4;
							const float alpha = color[3] * texel[3] / (255.0f * 255.0f);
							auto& pixel = pixels[row * IMAGE_WIDTH + column];
							for (int channel = 0; channel < 3; ++channel)
							{
								const float foreground = color[channel] * texel[channel] / 255.0f;
								pixel[channel] = static_cast<unsigned char>(std::clamp(foreground * alpha + pixel[channel] * (1 - alpha), 0.0f, 255.0f));
							}
						}
					}
				}
			}
			std::ofstream image{filename, std::ios::binary};
			image << "P6\n" << IMAGE_WIDTH << ' ' << IMAGE_HEIGHT << "\n255\n";
			image.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size() * 3));
			ASSERT_TRUE(image.good());
		}

		NatoGeometry::Symbol MakeVisualSymbol(NatoGeometry::SymbolSet set, NatoGeometry::StandardIdentity identity, bool labels)
		{
			NatoGeometry::SidcFields fields;
			fields.Identity = identity;
			fields.SymbolSetValue = set;
			fields.Entity = set == NatoGeometry::SymbolSet::SeaSurface ? 12 : 11;
			fields.EntityType = set == NatoGeometry::SymbolSet::SeaSurface ? 2 : 1;
			fields.EntitySubtype = set == NatoGeometry::SymbolSet::SeaSurface ? 4 : 0;
			NatoGeometry::Symbol symbol{*NatoGeometry::Sidc::Create(fields), {}, std::nullopt};
			if (labels)
			{
				symbol.ExternalAmplifiers.UniqueDesignation = "USS Example";
				symbol.ExternalAmplifiers.NameOrType = "Perry (FFG)";
			}
			return symbol;
		}
	}

	TEST(TacticalPresentationIntegration, RendersMeasuredSceneAndWritesVisualSmokeArtifacts)
	{
		ImGui::CreateContext();
		auto& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.DisplaySize = {IMAGE_WIDTH, IMAGE_HEIGHT};
		io.DeltaTime = 1.0f / 60;
		unsigned char* fontPixels{};
		int fontWidth{};
		int fontHeight{};
		io.Fonts->GetTexDataAsRGBA32(&fontPixels, &fontWidth, &fontHeight);
		ImGui::NewFrame();
		ImGui::SetNextWindowPos({0, 0});
		ImGui::SetNextWindowSize(io.DisplaySize);
		ImGui::Begin("Visual fixture", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
		auto& drawing = *ImGui::GetWindowDrawList();
		Symbology::Nato::ImGui::Renderer renderer;
		NatoGeometry::RenderContext context;
		context.Viewport = {{0, 0}, {IMAGE_WIDTH, IMAGE_HEIGHT}};
		context.SymbolScale = 1.6;
		context.ShowLabels = false;
		const std::array identities{NatoGeometry::StandardIdentity::Friendly,
									NatoGeometry::StandardIdentity::Hostile,
									NatoGeometry::StandardIdentity::Neutral,
									NatoGeometry::StandardIdentity::Unknown,
									NatoGeometry::StandardIdentity::AssumedFriendly};
		const std::array sets{NatoGeometry::SymbolSet::SeaSurface, NatoGeometry::SymbolSet::Air, NatoGeometry::SymbolSet::SeaSubsurface};
		for (int style = 0; style < 2; ++style)
		{
			context.Style = NatoGeometry::SymbolStyle::FromPreset(static_cast<NatoGeometry::StylePreset>(style));
			drawing.AddText(
				{20, static_cast<float>(20 + style * 310)}, IM_COL32_WHITE, style == 0 ? "Filled dark: friend / hostile / neutral / unknown / assumed friend" : "Unfilled dark");
			for (std::size_t row = 0; row < sets.size(); ++row)
			{
				for (std::size_t column = 0; column < identities.size(); ++column)
				{
					context.VisualAnchor = {100.0 + column * 190, 85.0 + row * 85 + style * 310};
					context.GeographicAnchor = context.VisualAnchor;
					renderer.Render(MakeVisualSymbol(sets[row], identities[column], false), context);
				}
			}
		}
		SaveImage(drawing, "symbol-styles.ppm");
		ImGui::End();
		ImGui::EndFrame();
		ImGui::NewFrame();
		ImGui::Begin("Visual fixture", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
		drawing.AddText({20, 20}, IM_COL32_WHITE, "Platform icons: carrier / guided missile cruiser / attack submarine / rotary / fighter / AEW / jammer");
		struct IconCase
		{
			NatoGeometry::SymbolSet Set;
			std::uint8_t EntityType;
			std::uint8_t Subtype;
			std::uint8_t Modifier;
		};
		const std::array iconCases{IconCase{NatoGeometry::SymbolSet::SeaSurface, 1, 0, 0},
								   IconCase{NatoGeometry::SymbolSet::SeaSurface, 2, 2, 33},
								   IconCase{NatoGeometry::SymbolSet::SeaSubsurface, 1, 0, 8},
								   IconCase{NatoGeometry::SymbolSet::Air, 2, 0, 0},
								   IconCase{NatoGeometry::SymbolSet::Air, 1, 0, 4},
								   IconCase{NatoGeometry::SymbolSet::Air, 1, 0, 12},
								   IconCase{NatoGeometry::SymbolSet::Air, 1, 0, 16}};
		context.Style = {};
		for (std::size_t row = 0; row < identities.size(); ++row)
		{
			for (std::size_t column = 0; column < iconCases.size(); ++column)
			{
				const auto& icon = iconCases[column];
				NatoGeometry::SidcFields fields;
				fields.Identity = identities[row];
				fields.SymbolSetValue = icon.Set;
				fields.Entity = icon.Set == NatoGeometry::SymbolSet::SeaSurface ? 12 : 11;
				fields.EntityType = icon.EntityType;
				fields.EntitySubtype = icon.Subtype;
				fields.Modifier1 = icon.Modifier;
				if (icon.Set == NatoGeometry::SymbolSet::SeaSurface && icon.Modifier == 33)
				{
					fields.ThirdTen[0] = '1';
				}
				const auto sidc = NatoGeometry::Sidc::Create(fields);
				ASSERT_TRUE(sidc);
				const NatoGeometry::Symbol symbol{*sidc, {}, std::nullopt};
				context.VisualAnchor = {75.0 + column * 140, 100.0 + row * 110};
				context.GeographicAnchor = context.VisualAnchor;
				EXPECT_TRUE(renderer.Render(symbol, context).WasDrawn);
			}
		}
		SaveImage(drawing, "platform-icons.ppm");
		ImGui::End();
		ImGui::EndFrame();
		ImGui::NewFrame();
		ImGui::Begin("Visual fixture", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
		drawing.AddText({20, 20}, IM_COL32_WHITE, "Six independent objects: overlapping true positions, measured type labels");
		SymbolLayout layout;
		std::vector<LayoutItem> items;
		for (int index = 0; index < 6; ++index)
		{
			items.push_back({{index % 2 == 0, std::to_string(index)}, {450.0 + index * 7, 300}, {}, {}, index == 0});
		}
		layout.Prepare(items, std::nullopt);
		context.Style = {};
		context.ShowLabels = true;
		context.DetailedLabels = false;
		for (auto& item : items)
		{
			context.VisualAnchor = item.VisualAnchor;
			context.GeographicAnchor = item.GeographicAnchor;
			item.Geometry = renderer.Measure(MakeVisualSymbol(sets[0], identities[0], true), context);
		}
		layout.Solve(items, context.Viewport);
		for (std::size_t first = 0; first < items.size(); ++first)
		{
			for (std::size_t second = first + 1; second < items.size(); ++second)
			{
				const auto left = items[first].Geometry.LayoutBounds;
				const auto right = items[second].Geometry.LayoutBounds;
				EXPECT_TRUE(left.Maximum.X <= right.Minimum.X || right.Maximum.X <= left.Minimum.X || left.Maximum.Y <= right.Minimum.Y || right.Maximum.Y <= left.Minimum.Y);
			}
		}

		for (const auto& item : items)
		{
			EXPECT_EQ(item.GeographicAnchor.Y, 300);
			const auto endpoint = ConnectorEndpoint(item.GeographicAnchor, item.Geometry.SymbolBounds);
			drawing.AddLine({static_cast<float>(item.GeographicAnchor.X), 300}, {static_cast<float>(endpoint.X), static_cast<float>(endpoint.Y)}, IM_COL32(150, 160, 170, 255));
		}
		const int firstVertex = drawing.VtxBuffer.Size;
		for (const auto& item : items)
		{
			context.VisualAnchor = item.VisualAnchor;
			context.GeographicAnchor = item.GeographicAnchor;
			const auto rendered = renderer.Render(MakeVisualSymbol(sets[0], identities[0], true), context);
			EXPECT_NEAR(rendered.LayoutBounds.Maximum.X, item.Geometry.LayoutBounds.Maximum.X, 0.001);
			EXPECT_NEAR(rendered.LayoutBounds.Maximum.Y, item.Geometry.LayoutBounds.Maximum.Y, 0.001);
			drawing.AddCircleFilled({static_cast<float>(item.GeographicAnchor.X), 300}, 2, IM_COL32(128, 224, 255, 255));
		}
		EXPECT_GT(drawing.VtxBuffer.Size, firstVertex);
		SaveImage(drawing, "measured-scene.ppm");
		layout.Persist(items);
		const auto hoverAnchor = items[0].VisualAnchor;
		const auto hoveredKey = items[0].Key;
		for (int frame = 0; frame < 12; ++frame)
		{
			layout.Prepare(items, hoverAnchor);
			for (auto& item : items)
			{
				context.VisualAnchor = item.VisualAnchor;
				context.GeographicAnchor = item.GeographicAnchor;
				context.ShowLabels = layout.IsHovered(item.Key);
				context.DetailedLabels = true;
				item.Geometry = renderer.Measure(MakeVisualSymbol(sets[0], identities[0], true), context);
			}
			layout.Solve(items, context.Viewport);
			EXPECT_TRUE(layout.IsHovered(hoveredKey));
			EXPECT_EQ(items[0].VisualAnchor.X, hoverAnchor.X);
			EXPECT_EQ(items[0].VisualAnchor.Y, hoverAnchor.Y);
			layout.Persist(items);
		}

		ImGui::End();
		ImGui::EndFrame();
		ImGui::DestroyContext();
	}
}
