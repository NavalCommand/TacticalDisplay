#include <NavalCommand/TacticalDisplay/SymbolLayout.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <vector>

namespace NavalCommand::TacticalDisplay
{
	namespace
	{
		using Point = NatoGeometry::ScreenPoint;
		using Rectangle = NatoGeometry::ScreenRectangle;
		constexpr Rectangle VIEWPORT{{0, 0}, {1200, 800}};

		LayoutItem MakeItem(std::string id, Point anchor, bool contact = false)
		{
			return {{contact, std::move(id)}, anchor, anchor, {}, false};
		}

		void Measure(std::vector<LayoutItem>& items, SymbolLayout& layout, bool hoverLabels = false, double labelWidth = 230)
		{
			for (auto& item : items)
			{
				const auto anchor = item.VisualAnchor;
				const auto core = Translate({{-20, -20}, {20, 20}}, {anchor.X, anchor.Y});
				const auto presentation = hoverLabels && layout.IsHovered(item.Key) ? Translate({{-20, -20}, {labelWidth, 110}}, {anchor.X, anchor.Y}) : core;
				item.Geometry = {presentation, core, presentation, {item.GeographicAnchor, item.GeographicAnchor}, true};
			}
		}

		void LayoutFrame(std::vector<LayoutItem>& items, SymbolLayout& layout, std::optional<Point> mouse = std::nullopt, bool labels = false)
		{
			layout.Prepare(items, mouse);
			Measure(items, layout, labels);
			layout.Solve(items, VIEWPORT);
			layout.Persist(items);
		}

		void ExpectSeparated(const std::vector<LayoutItem>& items)
		{
			for (std::size_t first = 0; first < items.size(); ++first)
			{
				for (std::size_t second = first + 1; second < items.size(); ++second)
				{
					const auto left = items[first].Geometry.LayoutBounds;
					const auto right = items[second].Geometry.LayoutBounds;
					EXPECT_TRUE(left.Maximum.X <= right.Minimum.X || right.Maximum.X <= left.Minimum.X || left.Maximum.Y <= right.Minimum.Y || right.Maximum.Y <= left.Minimum.Y)
						<< items[first].Key.Id << " / " << items[second].Key.Id;
				}
			}
		}
	}

	TEST(SymbolLayoutTest, SmallRelativeMotionDoesNotAlternateBetweenDistantPlacements)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("kiev", {500, 400}), MakeItem("kilo", {501, 401}), MakeItem("udaloy", {499, 401})};
		LayoutFrame(items, layout);
		for (int frame = 0; frame < 240; ++frame)
		{
			const auto previous = items;
			for (std::size_t index = 0; index < items.size(); ++index)
			{
				items[index].GeographicAnchor.X += index == 0 ? 0.02 : frame % 2 == 0 ? 0.2 : -0.2;
				items[index].GeographicAnchor.Y += frame % 2 == 0 ? 0.1 : -0.1;
			}
			LayoutFrame(items, layout);
			ExpectSeparated(items);
			for (std::size_t index = 0; index < items.size(); ++index)
			{
				EXPECT_LT(std::abs(items[index].VisualAnchor.X - previous[index].VisualAnchor.X), 12.0);
				EXPECT_LT(std::abs(items[index].VisualAnchor.Y - previous[index].VisualAnchor.Y), 12.0);
			}
		}
	}

	TEST(SymbolLayoutTest, NewlyLaunchedAircraftDoesNotKeepSwappingWithCarrier)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("carrier", {500, 400})};
		LayoutFrame(items, layout);
		items.push_back(MakeItem("aircraft", {501, 400}));
		LayoutFrame(items, layout);
		for (int frame = 0; frame < 120; ++frame)
		{
			const auto previous = items;
			items[0].GeographicAnchor.X += frame % 2 == 0 ? 0.15 : -0.15;
			LayoutFrame(items, layout);
			ExpectSeparated(items);
			for (std::size_t index = 0; index < items.size(); ++index)
			{
				EXPECT_LT(std::abs(items[index].VisualAnchor.X - previous[index].VisualAnchor.X), 12.0);
				EXPECT_LT(std::abs(items[index].VisualAnchor.Y - previous[index].VisualAnchor.Y), 12.0);
			}
		}
	}

	TEST(SymbolLayoutTest, SelectionDoesNotRearrangeUnchangedSymbolGeometry)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {500, 400}), MakeItem("second", {500, 400}), MakeItem("third", {500, 400})};
		LayoutFrame(items, layout);
		const auto initial = items;
		for (std::size_t selected = 0; selected < items.size(); ++selected)
		{
			items[selected].Selected = true;
			LayoutFrame(items, layout);
			for (std::size_t index = 0; index < items.size(); ++index)
			{
				EXPECT_DOUBLE_EQ(items[index].VisualAnchor.X, initial[index].VisualAnchor.X);
				EXPECT_DOUBLE_EQ(items[index].VisualAnchor.Y, initial[index].VisualAnchor.Y);
			}
			items[selected].Selected = false;
		}
	}

	TEST(SymbolLayoutTest, OnlyDisplacedSymbolsShowPositionMarkers)
	{
		auto item = MakeItem("single", {500, 400});
		EXPECT_FALSE(HasDeclutteringOffset(item));
		item.VisualAnchor.X += 40;
		EXPECT_TRUE(HasDeclutteringOffset(item));
	}

	TEST(SymbolLayoutTest, ScaleReductionReclaimsOffsetsEvenForSelectedSymbols)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {500, 400}), MakeItem("second", {500, 400}), MakeItem("third", {500, 400})};
		items[1].Selected = true;
		LayoutFrame(items, layout);
		const auto small = items;
		layout.Prepare(items, std::nullopt);
		for (auto& item : items)
		{
			const auto bounds = Translate({{-80, -80}, {80, 80}}, {item.VisualAnchor.X, item.VisualAnchor.Y});
			item.Geometry.SymbolBounds = bounds;
			item.Geometry.LayoutBounds = bounds;
		}
		layout.Solve(items, VIEWPORT);
		layout.Persist(items);
		LayoutFrame(items, layout);
		for (std::size_t index = 0; index < items.size(); ++index)
		{
			EXPECT_DOUBLE_EQ(items[index].VisualAnchor.X, small[index].VisualAnchor.X);
			EXPECT_DOUBLE_EQ(items[index].VisualAnchor.Y, small[index].VisualAnchor.Y);
		}
		ExpectSeparated(items);
	}

	TEST(SymbolLayoutTest, HoverCollapseReclaimsOffsetsWhenHoveredKeyWasLast)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {520, 350}), MakeItem("second", {450, 350})};
		LayoutFrame(items, layout);
		const auto initial = items;
		LayoutFrame(items, layout, items[1].VisualAnchor, true);
		EXPECT_TRUE(HasDeclutteringOffset(items[0]));
		LayoutFrame(items, layout);
		for (std::size_t index = 0; index < items.size(); ++index)
		{
			EXPECT_DOUBLE_EQ(items[index].VisualAnchor.X, initial[index].VisualAnchor.X);
			EXPECT_DOUBLE_EQ(items[index].VisualAnchor.Y, initial[index].VisualAnchor.Y);
		}
	}

	TEST(SymbolLayoutTest, DisplacedLayoutsDoNotCoverAnyPositionMarker)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {500, 400}), MakeItem("second", {505, 402}), MakeItem("third", {510, 400})};
		LayoutFrame(items, layout);
		ExpectSeparated(items);
		for (const auto& item : items)
		{
			ASSERT_TRUE(HasDeclutteringOffset(item));
			for (const auto& other : items)
			{
				const auto marker = PositionMarkerBounds(other.GeographicAnchor);
				const auto bounds = item.Geometry.LayoutBounds;
				EXPECT_TRUE(bounds.Maximum.X <= marker.Minimum.X || bounds.Minimum.X >= marker.Maximum.X || bounds.Maximum.Y <= marker.Minimum.Y ||
							bounds.Minimum.Y >= marker.Maximum.Y);
			}
		}
	}

	TEST(SymbolLayoutTest, SeparatesIdenticalPositionsAndMixedNamespacesDeterministically)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("same", {500, 400}), MakeItem("same", {500, 400}, true), MakeItem("third", {505, 402})};
		LayoutFrame(items, layout);
		ExpectSeparated(items);
		ASSERT_EQ(items.size(), 3);
		EXPECT_FALSE(items[0].Key.Contact);
		EXPECT_TRUE(items[2].Key.Contact);
		const auto previous = items;
		std::ranges::reverse(items);
		LayoutFrame(items, layout);
		for (std::size_t index = 0; index < items.size(); ++index)
		{
			EXPECT_EQ(items[index].VisualAnchor.X, previous[index].VisualAnchor.X);
			EXPECT_EQ(items[index].VisualAnchor.Y, previous[index].VisualAnchor.Y);
			EXPECT_EQ(items[index].GeographicAnchor.X, previous[index].GeographicAnchor.X);
		}
	}

	TEST(SymbolLayoutTest, HoverExpansionPinsDisplayedIconAndMovesItsNeighbor)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {450, 350}), MakeItem("second", {510, 350})};
		LayoutFrame(items, layout);
		const auto firstAnchor = items[0].VisualAnchor;
		LayoutFrame(items, layout, firstAnchor, true);
		EXPECT_TRUE(layout.IsHovered(items[0].Key));
		EXPECT_EQ(items[0].VisualAnchor.X, firstAnchor.X);
		EXPECT_EQ(items[0].VisualAnchor.Y, firstAnchor.Y);
		ExpectSeparated(items);
		const auto expanded = items;
		for (int frame = 0; frame < 20; ++frame)
		{
			LayoutFrame(items, layout, firstAnchor, true);
			EXPECT_TRUE(layout.IsHovered(items[0].Key));
			EXPECT_EQ(items[0].VisualAnchor.X, firstAnchor.X);
			EXPECT_EQ(items[1].VisualAnchor.X, expanded[1].VisualAnchor.X);
			EXPECT_EQ(items[1].VisualAnchor.Y, expanded[1].VisualAnchor.Y);
		}
		LayoutFrame(items, layout);
		EXPECT_EQ(items[1].VisualAnchor.X, items[1].GeographicAnchor.X);
		EXPECT_EQ(items[1].VisualAnchor.Y, items[1].GeographicAnchor.Y);
	}

	TEST(SymbolLayoutTest, RichnessChangesAndSelectedNeighborNeverMoveHoveredSymbol)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {450, 350}), MakeItem("second", {510, 350})};
		items[1].Selected = true;
		LayoutFrame(items, layout);
		const auto anchor = items[0].VisualAnchor;
		layout.Prepare(items, anchor);
		Measure(items, layout, true, 450);
		layout.Solve(items, VIEWPORT);
		layout.Persist(items);
		ExpectSeparated(items);
		EXPECT_EQ(items[0].VisualAnchor.X, anchor.X);
		EXPECT_EQ(items[0].VisualAnchor.Y, anchor.Y);
		// Labels are not hit targets.
		layout.Prepare(items, Point{anchor.X + 300, anchor.Y});
		EXPECT_FALSE(layout.IsHovered(items[0].Key));
	}

	TEST(SymbolLayoutTest, LabelOnlyCollisionsAreSeparatedUsingMeasuredRectangles)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {400, 300}), MakeItem("second", {550, 300})};
		layout.Prepare(items, std::nullopt);
		Measure(items, layout);
		for (auto& item : items)
		{
			item.Geometry.LayoutBounds.Maximum.X += 220;
			item.Geometry.LayoutBounds.Maximum.Y += 90;
		}
		layout.Solve(items, VIEWPORT);
		ExpectSeparated(items);
		EXPECT_EQ(items[0].GeographicAnchor.X, 400);
		EXPECT_EQ(items[1].GeographicAnchor.X, 550);
	}

	TEST(SymbolLayoutTest, PanPreservesOffsetsAndZoomAllowsReturnToGeographicAnchors)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {450, 350}), MakeItem("second", {460, 350})};
		LayoutFrame(items, layout);
		const auto prior = items;
		for (auto& item : items)
		{
			item.GeographicAnchor.X += 80;
			item.GeographicAnchor.Y += 60;
		}
		LayoutFrame(items, layout);
		for (std::size_t index = 0; index < items.size(); ++index)
		{
			EXPECT_EQ(items[index].VisualAnchor.X, prior[index].VisualAnchor.X + 80);
			EXPECT_EQ(items[index].VisualAnchor.Y, prior[index].VisualAnchor.Y + 60);
		}
		items[1].GeographicAnchor.X += 300;
		LayoutFrame(items, layout);
		EXPECT_EQ(items[1].VisualAnchor.X, items[1].GeographicAnchor.X);
		EXPECT_EQ(items[1].VisualAnchor.Y, items[1].GeographicAnchor.Y);
	}

	TEST(SymbolLayoutTest, ViewportEdgeAndChangingClassificationRemainAccessible)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {5, 5}), MakeItem("second", {6, 6}), MakeItem("third", {7, 7})};
		LayoutFrame(items, layout);
		ExpectSeparated(items);
		layout.Prepare(items, std::nullopt);
		Measure(items, layout);
		items[0].Geometry.LayoutBounds.Maximum.X += 150;
		layout.Solve(items, VIEWPORT);
		ExpectSeparated(items);
		for (const auto& item : items)
		{
			EXPECT_GE(item.Geometry.LayoutBounds.Minimum.X, 0);
			EXPECT_GE(item.Geometry.LayoutBounds.Minimum.Y, 0);
		}
	}

	TEST(SymbolLayoutTest, HoverTransitionsUseDisplayedIconsAndScopeResetDropsOldState)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("first", {450, 350}), MakeItem("second", {450, 350})};
		LayoutFrame(items, layout);
		for (int frame = 0; frame < 10; ++frame)
		{
			const auto hoveredIndex = static_cast<std::size_t>(frame % 2);
			const auto anchor = items[hoveredIndex].VisualAnchor;
			LayoutFrame(items, layout, anchor, true);
			EXPECT_TRUE(layout.IsHovered(items[hoveredIndex].Key));
			EXPECT_EQ(items[hoveredIndex].VisualAnchor.X, anchor.X);
			EXPECT_EQ(items[hoveredIndex].VisualAnchor.Y, anchor.Y);
		}
		layout.Clear();
		layout.Prepare(items, Point{0, 0});
		EXPECT_FALSE(layout.IsHovered(items[0].Key));
		EXPECT_EQ(items[1].VisualAnchor.X, items[1].GeographicAnchor.X);
	}

	TEST(SymbolLayoutTest, DenseScenesAreBoundedAndPreserveEveryIdentity)
	{
		SymbolLayout layout;
		std::vector<LayoutItem> items;
		for (int index = 0; index < 300; ++index)
		{
			items.push_back(MakeItem(std::to_string(index), {600, 400}));
		}
		const auto started = std::chrono::steady_clock::now();
		LayoutFrame(items, layout);
		RecordProperty("layout_milliseconds", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
		const auto repeated = std::chrono::steady_clock::now();
		for (int frame = 0; frame < 20; ++frame)
		{
			LayoutFrame(items, layout);
		}
		RecordProperty("cached_20_frames_milliseconds", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - repeated).count());
		EXPECT_EQ(items.size(), 300);
		for (const auto& item : items)
		{
			EXPECT_EQ(item.GeographicAnchor.X, 600);
			EXPECT_EQ(item.GeographicAnchor.Y, 400);
			EXPECT_TRUE(VIEWPORT.Contains(item.VisualAnchor));
		}
	}

	TEST(SymbolLayoutTest, ConnectorEndsAtBoundaryAndNeverChangesTruePosition)
	{
		const Rectangle icon{{200, 200}, {240, 240}};
		const Point actual{100, 220};
		const auto end = ConnectorEndpoint(actual, icon);
		EXPECT_EQ(end.X, 200);
		EXPECT_EQ(end.Y, 220);
		EXPECT_EQ(actual.X, 100);
		EXPECT_EQ(actual.Y, 220);
		EXPECT_EQ(ConnectorEndpoint({220, 220}, icon).X, 220);
	}
	TEST(SymbolLayoutTest, ClickTargetsDisplayedCoreWithDistinctUnitAndContactIdentity)
	{
		SymbolLayout layout;
		std::vector items{MakeItem("same", {450, 350}), MakeItem("same", {450, 350}, true)};
		LayoutFrame(items, layout);
		for (const auto& item : items)
		{
			const auto hit = layout.HitTest(items, item.VisualAnchor);
			ASSERT_TRUE(hit);
			EXPECT_EQ(*hit, item.Key);
		}
		EXPECT_FALSE(layout.HitTest(items, {100, 100}));
	}

}
