#pragma once

#include <NavalCommand/Symbology/Nato/Api/DomainConverter.h>
#include <NavalCommand/Symbology/Nato/Api/IRenderer.h>
#include <string>

namespace NavalCommand::TacticalDisplay
{
	// Presentation recovery is explicit; strict domain conversion remains unchanged.
	struct SymbolPresentation
	{
		Symbology::Nato::Api::Symbol Value;
		std::string Diagnostic;

		static SymbolPresentation FromUnit(const Units::Api::Instance& unit,
										   const std::optional<Units::Api::Type>& type,
										   Symbology::Nato::Api::StandardIdentity identity,
										   Symbology::Nato::Api::Context context);
		static SymbolPresentation FromObservation(const Observations::Api::Instance& observation,
												  const std::optional<Units::Api::Type>& type,
												  Symbology::Nato::Api::StandardIdentity identity,
												  Symbology::Nato::Api::Context context);
		void UseGenericSymbol(std::string diagnostic);
		Symbology::Nato::Api::MeasureResult Measure(Symbology::Nato::Api::IRenderer& renderer, const Symbology::Nato::Api::RenderContext& context);
	};
}
