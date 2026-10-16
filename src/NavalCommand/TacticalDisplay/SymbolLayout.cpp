#include <NavalCommand/TacticalDisplay/SymbolLayout.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <vector>

namespace NavalCommand::TacticalDisplay
{
	namespace
	{
		constexpr double COLLISION_GAP_PIXELS = 4.0;
		constexpr std::size_t MAX_CANDIDATES = 192;
		constexpr double PLACEMENT_DEADBAND_PIXELS = 8.0;
		constexpr double REPAIR_CLEARANCE_PIXELS = 4.0;
		using Point = NatoGeometry::ScreenPoint;
		using Rectangle = NatoGeometry::ScreenRectangle;

		bool Intersects(Rectangle first, Rectangle second)
		{
			return first.Minimum.X < second.Maximum.X && first.Maximum.X > second.Minimum.X && first.Minimum.Y < second.Maximum.Y && first.Maximum.Y > second.Minimum.Y;
		}

		Rectangle Padded(Rectangle bounds)
		{
			bounds.Minimum.X -= COLLISION_GAP_PIXELS * 0.5;
			bounds.Minimum.Y -= COLLISION_GAP_PIXELS * 0.5;
			bounds.Maximum.X += COLLISION_GAP_PIXELS * 0.5;
			bounds.Maximum.Y += COLLISION_GAP_PIXELS * 0.5;
			return bounds;
		}

		double DistanceSquared(Point first, Point second)
		{
			const double horizontal = first.X - second.X;
			const double vertical = first.Y - second.Y;
			return horizontal * horizontal + vertical * vertical;
		}

		Point Constrain(Point anchor, Rectangle relativeBounds, Rectangle viewport)
		{
			const double left = viewport.Minimum.X - relativeBounds.Minimum.X;
			const double right = viewport.Maximum.X - relativeBounds.Maximum.X;
			const double top = viewport.Minimum.Y - relativeBounds.Minimum.Y;
			const double bottom = viewport.Maximum.Y - relativeBounds.Maximum.Y;
			// Oversize labels cannot fit. Keep their origin accessible and allow
			// clipping rather than oscillating between opposite viewport edges.
			return {std::clamp(anchor.X, left, std::max(left, right)), std::clamp(anchor.Y, top, std::max(top, bottom))};
		}

		struct Candidate
		{
			Point Anchor;
			double Cost{};
			bool operator<(const Candidate& other) const
			{
				if (Cost != other.Cost)
				{
					return Cost > other.Cost;
				}
				if (Anchor.Y != other.Anchor.Y)
				{
					return Anchor.Y > other.Anchor.Y;
				}
				return Anchor.X > other.Anchor.X;
			}
		};

		Point Place(const LayoutItem& item, Rectangle viewport, const std::vector<Rectangle>& placed, std::span<const LayoutItem> items, bool stabilize, bool sticky)
		{
			const auto relative = Padded(Translate(item.Geometry.LayoutBounds, {-item.VisualAnchor.X, -item.VisualAnchor.Y}));
			const Point preferred = item.GeographicAnchor;
			std::vector<Rectangle> obstacles = placed;
			obstacles.reserve(placed.size() + items.size());
			std::set<std::pair<double, double>> markerCoordinates;
			bool sharedMarker = false;
			for (const auto& other : items)
			{
				if (other.Key != item.Key && markerCoordinates.emplace(other.GeographicAnchor.X, other.GeographicAnchor.Y).second)
				{
					obstacles.push_back(PositionMarkerBounds(other.GeographicAnchor));
					sharedMarker = sharedMarker || DistanceSquared(other.GeographicAnchor, item.GeographicAnchor) < 0.01;
				}
			}
			// A centered symbol hides its own marker. A displaced one must leave
			// space for it, including when returning toward its geographic anchor.
			if (!sharedMarker)
			{
				obstacles.push_back(PositionMarkerBounds(item.GeographicAnchor));
			}
			const auto overlaps = [&](Point anchor, Rectangle bounds, std::size_t index)
			{
				if (!sharedMarker && index + 1 == obstacles.size() && DistanceSquared(anchor, item.GeographicAnchor) < 0.01)
				{
					return false;
				}
				return Intersects(bounds, obstacles[index]);
			};
			const auto clear = [&](Point anchor, double clearance = 0.0)
			{
				auto bounds = Translate(relative, {anchor.X, anchor.Y});
				bounds.Minimum.X -= clearance;
				bounds.Minimum.Y -= clearance;
				bounds.Maximum.X += clearance;
				bounds.Maximum.Y += clearance;
				for (std::size_t index = 0; index < obstacles.size(); ++index)
				{
					if (overlaps(anchor, bounds, index))
					{
						return false;
					}
				}
				return true;
			};
			const auto desired = Constrain(preferred, relative, viewport);
			if (!stabilize && clear(desired))
			{
				return desired;
			}
			const auto previous = Constrain(item.VisualAnchor, relative, viewport);
			const bool previousClear = stabilize && clear(previous);
			if (sticky && previousClear)
			{
				return previous;
			}

			std::priority_queue<Candidate> candidates;
			std::set<std::pair<double, double>> visited;
			const auto enqueue = [&](Point candidate)
			{
				candidate = Constrain(candidate, relative, viewport);
				if (visited.size() < MAX_CANDIDATES * 4 && visited.emplace(candidate.X, candidate.Y).second)
				{
					const double repairCost = stabilize && !previousClear ? 4.0 * DistanceSquared(candidate, previous) : 0.0;
					candidates.push({candidate, DistanceSquared(candidate, preferred) + repairCost});
				}
			};
			enqueue(desired);
			enqueue(previous);
			Point best = desired;
			double bestOverlap = INFINITY;
			for (std::size_t iteration = 0; iteration < MAX_CANDIDATES && !candidates.empty(); ++iteration)
			{
				const auto candidate = candidates.top();
				candidates.pop();
				const auto bounds = Translate(relative, {candidate.Anchor.X, candidate.Anchor.Y});
				double overlapArea = 0.0;
				for (std::size_t index = 0; index < obstacles.size(); ++index)
				{
					const auto obstacle = obstacles[index];
					if (!overlaps(candidate.Anchor, bounds, index))
					{
						continue;
					}
					overlapArea += (std::min(bounds.Maximum.X, obstacle.Maximum.X) - std::max(bounds.Minimum.X, obstacle.Minimum.X)) *
								   (std::min(bounds.Maximum.Y, obstacle.Maximum.Y) - std::max(bounds.Minimum.Y, obstacle.Minimum.Y));
					// Exact separating translations along each rectangle boundary.
					const double clearance = stabilize ? REPAIR_CLEARANCE_PIXELS : 0.0;
					enqueue({obstacle.Minimum.X - relative.Maximum.X - clearance, candidate.Anchor.Y});
					enqueue({obstacle.Maximum.X - relative.Minimum.X + clearance, candidate.Anchor.Y});
					enqueue({candidate.Anchor.X, obstacle.Minimum.Y - relative.Maximum.Y - clearance});
					enqueue({candidate.Anchor.X, obstacle.Maximum.Y - relative.Minimum.Y + clearance});
				}
				if (overlapArea == 0.0)
				{
					if (previousClear)
					{
						const double improvement = std::sqrt(DistanceSquared(previous, preferred)) - std::sqrt(DistanceSquared(candidate.Anchor, preferred));
						const double movement = std::sqrt(DistanceSquared(previous, candidate.Anchor));
						if (improvement < PLACEMENT_DEADBAND_PIXELS + movement * 0.25 || !clear(candidate.Anchor, REPAIR_CLEARANCE_PIXELS))
						{
							return previous;
						}
					}
					return candidate.Anchor;
				}
				if (overlapArea < bestOverlap)
				{
					bestOverlap = overlapArea;
					best = candidate.Anchor;
				}
			}
			return best;
		}
	}

	NatoGeometry::ScreenRectangle PositionMarkerBounds(Point anchor)
	{
		constexpr double MARKER_RADIUS_PIXELS = 3.2;
		return {{anchor.X - MARKER_RADIUS_PIXELS, anchor.Y - MARKER_RADIUS_PIXELS}, {anchor.X + MARKER_RADIUS_PIXELS, anchor.Y + MARKER_RADIUS_PIXELS}};
	}

	bool HasDeclutteringOffset(const LayoutItem& item)
	{
		return DistanceSquared(item.GeographicAnchor, item.VisualAnchor) >= 0.01;
	}

	NatoGeometry::ScreenRectangle Translate(NatoGeometry::ScreenRectangle bounds, NatoGeometry::ScreenVector offset)
	{
		return {{bounds.Minimum.X + offset.X, bounds.Minimum.Y + offset.Y}, {bounds.Maximum.X + offset.X, bounds.Maximum.Y + offset.Y}};
	}

	void SymbolLayout::Clear()
	{
		m_Previous.clear();
		m_Hovered.reset();
		m_LastViewport.reset();
	}

	void SymbolLayout::Prepare(std::span<LayoutItem> items, std::optional<Point> mouse)
	{
		std::ranges::sort(items, {}, &LayoutItem::Key);
		std::optional<SymbolKey> hovered;
		for (auto& item : items)
		{
			item.VisualAnchor = item.GeographicAnchor;
			const auto previous = m_Previous.find(item.Key);
			if (previous == m_Previous.end())
			{
				continue;
			}
			const NatoGeometry::ScreenVector shift{item.GeographicAnchor.X - previous->second.GeographicAnchor.X, item.GeographicAnchor.Y - previous->second.GeographicAnchor.Y};
			item.VisualAnchor = {previous->second.VisualAnchor.X + shift.X, previous->second.VisualAnchor.Y + shift.Y};
			if (mouse && Translate(previous->second.SymbolBounds, shift).Contains(*mouse))
			{
				if (!hovered || (m_Hovered && item.Key == *m_Hovered))
				{
					hovered = item.Key;
				}
			}
		}
		m_Hovered = std::move(hovered);
	}

	bool SymbolLayout::IsHovered(const SymbolKey& key) const
	{
		return m_Hovered && *m_Hovered == key;
	}

	std::optional<SymbolKey> SymbolLayout::HitTest(std::span<const LayoutItem> items, Point mouse) const
	{
		std::optional<SymbolKey> hit;
		for (const auto& item : items)
		{
			if (item.Geometry.SymbolBounds.Contains(mouse))
			{
				if (IsHovered(item.Key))
				{
					return item.Key;
				}
				if (!hit || item.Key < *hit)
				{
					hit = item.Key;
				}
			}
		}
		return hit;
	}

	void SymbolLayout::Solve(std::span<LayoutItem> items, Rectangle viewport, DeclutteringMode mode)
	{
		const auto sameRectangle = [](Rectangle first, Rectangle second)
		{
			return std::abs(first.Minimum.X - second.Minimum.X) < 0.001 && std::abs(first.Minimum.Y - second.Minimum.Y) < 0.001 &&
				   std::abs(first.Maximum.X - second.Maximum.X) < 0.001 && std::abs(first.Maximum.Y - second.Maximum.Y) < 0.001;
		};
		// Repeated provider frames need no collision search. Measurements still
		// reflect the current font, style, visibility and richness.
		if (m_LastViewport && sameRectangle(*m_LastViewport, viewport) && m_LastMode == mode && m_Previous.size() == items.size() &&
			std::ranges::all_of(items,
								[&](const LayoutItem& item)
								{
									const auto previous = m_Previous.find(item.Key);
									return previous != m_Previous.end() && previous->second.GeographicAnchor.X == item.GeographicAnchor.X &&
										   previous->second.GeographicAnchor.Y == item.GeographicAnchor.Y &&
										   sameRectangle(previous->second.SymbolBounds, item.Geometry.SymbolBounds) &&
										   sameRectangle(previous->second.LayoutBounds, item.Geometry.LayoutBounds);
								}))
		{
			return;
		}
		const bool preservePlacements = m_LastViewport && sameRectangle(*m_LastViewport, viewport) && m_LastMode == mode &&
										std::ranges::all_of(items,
															[&](const LayoutItem& item)
															{
																const auto previous = m_Previous.find(item.Key);
																if (previous == m_Previous.end())
																{
																	return true;
																}
																const NatoGeometry::ScreenVector shift{item.VisualAnchor.X - previous->second.VisualAnchor.X,
																									   item.VisualAnchor.Y - previous->second.VisualAnchor.Y};
																const auto priorLayout = Translate(previous->second.LayoutBounds, shift);
																// Scale/icon changes and label visibility changes reflow immediately.
																// Small changing numeric label widths retain placement hysteresis.
																return sameRectangle(Translate(previous->second.SymbolBounds, shift), item.Geometry.SymbolBounds) &&
																	   std::abs(priorLayout.Minimum.X - item.Geometry.LayoutBounds.Minimum.X) <= PLACEMENT_DEADBAND_PIXELS &&
																	   std::abs(priorLayout.Minimum.Y - item.Geometry.LayoutBounds.Minimum.Y) <= PLACEMENT_DEADBAND_PIXELS &&
																	   std::abs(priorLayout.Maximum.X - item.Geometry.LayoutBounds.Maximum.X) <= PLACEMENT_DEADBAND_PIXELS &&
																	   std::abs(priorLayout.Maximum.Y - item.Geometry.LayoutBounds.Maximum.Y) <= PLACEMENT_DEADBAND_PIXELS;
															});
		m_LastViewport = viewport;
		m_LastMode = mode;
		std::vector<LayoutItem*> ordered;
		ordered.reserve(items.size());
		for (auto& item : items)
		{
			ordered.push_back(&item);
		}
		std::ranges::sort(ordered,
						  [&](const LayoutItem* first, const LayoutItem* second)
						  {
							  const int firstPriority = IsHovered(first->Key) ? 1 : 0;
							  const int secondPriority = IsHovered(second->Key) ? 1 : 0;
							  return firstPriority != secondPriority ? firstPriority > secondPriority : first->Key < second->Key;
						  });
		std::vector<Rectangle> obstacles;
		obstacles.reserve(items.size());
		for (auto* item : ordered)
		{
			const bool stabilize = (mode == DeclutteringMode::Sticky || preservePlacements) && m_Previous.contains(item->Key);
			const auto anchor = IsHovered(item->Key) ? item->VisualAnchor : Place(*item, viewport, obstacles, items, stabilize, mode == DeclutteringMode::Sticky);
			const NatoGeometry::ScreenVector shift{anchor.X - item->VisualAnchor.X, anchor.Y - item->VisualAnchor.Y};
			item->Geometry.LayoutBounds = Translate(item->Geometry.LayoutBounds, shift);
			item->Geometry.SymbolBounds = Translate(item->Geometry.SymbolBounds, shift);
			item->VisualAnchor = anchor;
			obstacles.push_back(Padded(item->Geometry.LayoutBounds));
		}
	}

	void SymbolLayout::Persist(std::span<const LayoutItem> items)
	{
		for (auto& [key, placement] : m_Previous)
		{
			placement.Seen = false;
		}
		for (const auto& item : items)
		{
			m_Previous.insert_or_assign(item.Key, Placement{item.GeographicAnchor, item.VisualAnchor, item.Geometry.SymbolBounds, item.Geometry.LayoutBounds, true});
		}
		std::erase_if(m_Previous,
					  [](const auto& entry)
					  {
						  return !entry.second.Seen;
					  });
	}

	NatoGeometry::ScreenPoint ConnectorEndpoint(Point geographicAnchor, Rectangle bounds)
	{
		const Point center{(bounds.Minimum.X + bounds.Maximum.X) * 0.5, (bounds.Minimum.Y + bounds.Maximum.Y) * 0.5};
		const double horizontal = geographicAnchor.X - center.X;
		const double vertical = geographicAnchor.Y - center.Y;
		const double halfWidth = (bounds.Maximum.X - bounds.Minimum.X) * 0.5;
		const double halfHeight = (bounds.Maximum.Y - bounds.Minimum.Y) * 0.5;
		const double fraction =
			std::min(std::abs(horizontal) > 0.0 ? halfWidth / std::abs(horizontal) : INFINITY, std::abs(vertical) > 0.0 ? halfHeight / std::abs(vertical) : INFINITY);
		if (bounds.Contains(geographicAnchor) || !std::isfinite(fraction))
		{
			return geographicAnchor;
		}
		return {center.X + horizontal * fraction, center.Y + vertical * fraction};
	}
}
