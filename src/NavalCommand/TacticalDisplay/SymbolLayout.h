#pragma once

#include <NavalCommand/Symbology/Nato/Api/MeasureResult.h>
#include <compare>
#include <map>
#include <optional>
#include <span>
#include <string>

namespace NavalCommand::TacticalDisplay
{
	namespace NatoGeometry = Symbology::Nato::Api;

	// Internal production presentation state. Unit and observation IDs never alias.
	struct SymbolKey
	{
		bool Contact{};
		std::string Id;
		auto operator<=>(const SymbolKey&) const = default;
	};

	struct LayoutItem
	{
		SymbolKey Key;
		NatoGeometry::ScreenPoint GeographicAnchor;
		NatoGeometry::ScreenPoint VisualAnchor;
		NatoGeometry::MeasureResult Geometry;
		bool Selected{};
	};

	class SymbolLayout
	{
		struct Placement
		{
			NatoGeometry::ScreenPoint GeographicAnchor;
			NatoGeometry::ScreenPoint VisualAnchor;
			NatoGeometry::ScreenRectangle SymbolBounds;
			NatoGeometry::ScreenRectangle LayoutBounds;
			bool Seen{};
		};
		std::map<SymbolKey, Placement> m_Previous;
		std::optional<SymbolKey> m_Hovered;
		std::optional<SymbolKey> m_LastSolvedHover;
		std::optional<NatoGeometry::ScreenRectangle> m_LastViewport;

	public:
		void Clear();
		// Restores offsets for pan and object motion. Hover is resolved against the
		// displayed core, before expanded labels participate in the new layout.
		void Prepare(std::span<LayoutItem> items, std::optional<NatoGeometry::ScreenPoint> mouse);
		bool IsHovered(const SymbolKey& key) const;
		std::optional<SymbolKey> HitTest(std::span<const LayoutItem> items, NatoGeometry::ScreenPoint mouse) const;
		void Solve(std::span<LayoutItem> items, NatoGeometry::ScreenRectangle viewport);
		void Persist(std::span<const LayoutItem> items);
	};

	NatoGeometry::ScreenRectangle PositionMarkerBounds(NatoGeometry::ScreenPoint anchor);
	bool HasDeclutteringOffset(const LayoutItem& item);
	NatoGeometry::ScreenRectangle Translate(NatoGeometry::ScreenRectangle bounds, NatoGeometry::ScreenVector offset);
	// Returns the first rectangle boundary met from the external geographic dot.
	NatoGeometry::ScreenPoint ConnectorEndpoint(NatoGeometry::ScreenPoint geographicAnchor, NatoGeometry::ScreenRectangle iconBounds);
}
