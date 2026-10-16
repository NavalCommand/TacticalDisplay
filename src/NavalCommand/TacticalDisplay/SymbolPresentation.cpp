#include <NavalCommand/TacticalDisplay/SymbolPresentation.h>
#include <utility>

namespace NavalCommand::TacticalDisplay
{
	namespace
	{
		namespace Nato = Symbology::Nato::Api;

		std::string TypeLabel(const Units::Api::TypeId& id, const std::optional<Units::Api::Type>& type)
		{
			if (!type || type->Id != id)
			{
				return id.Value;
			}
			if (!type->DisplayName)
			{
				return type->AlternativeDisplayName.value_or(id.Value);
			}
			return *type->DisplayName + (type->AlternativeDisplayName ? " (" + *type->AlternativeDisplayName + ")" : "");
		}

		std::string ConversionDiagnostic(const Nato::ConversionError& error)
		{
			const char* category = "Unexpected conversion error";
			switch (error.Code)
			{
			case Nato::ConversionErrorCode::UnsupportedClassification:
				category = "Unrepresentable classification or primary role";
				break;
			case Nato::ConversionErrorCode::InvalidDomainValue:
				category = "Invalid domain value";
				break;
			case Nato::ConversionErrorCode::MismatchedTypeDefinition:
				category = "Mismatched type definition";
				break;
			case Nato::ConversionErrorCode::InvalidSidc:
				category = "Invalid NATO modifier combination";
				break;
			}
			return std::string{category} + ": " + error.Message;
		}
	}

	void SymbolPresentation::UseGenericSymbol(std::string diagnostic)
	{
		Nato::SidcFields fields;
		const auto& original = Value.Identification.Fields();
		fields.ContextValue = original.ContextValue;
		fields.Identity = original.Identity;
		fields.Status = original.Status;
		// Unknown symbol set assigns no military dimension, entity or role.
		Value.Identification = Nato::Sidc::Create(fields).value();
		Diagnostic = std::move(diagnostic);
	}

	Nato::MeasureResult SymbolPresentation::Measure(Nato::IRenderer& renderer, const Nato::RenderContext& context)
	{
		auto geometry = renderer.Measure(Value, context);
		if (!geometry.Supported)
		{
			UseGenericSymbol("NATO renderer does not support this symbol's rendering details.");
			geometry = renderer.Measure(Value, context);
		}
		return geometry;
	}

	SymbolPresentation
	SymbolPresentation::FromUnit(const Units::Api::Instance& unit, const std::optional<Units::Api::Type>& type, Nato::StandardIdentity identity, Nato::Context context)
	{
		auto converted = Nato::DomainConverter::FromUnit(unit, type, identity, context);
		if (converted)
		{
			return {std::move(*converted), {}};
		}
		// Reuse the converter's unclassified metadata/kinematics path. No source facts change.
		auto generic = Nato::DomainConverter::FromUnit(unit, std::nullopt, identity, context).value();
		generic.ExternalAmplifiers.NameOrType = TypeLabel(unit.Type, type);
		return {std::move(generic), ConversionDiagnostic(converted.error())};
	}

	SymbolPresentation SymbolPresentation::FromObservation(const Observations::Api::Instance& observation,
														   const std::optional<Units::Api::Type>& type,
														   Nato::StandardIdentity identity,
														   Nato::Context context)
	{
		auto converted = Nato::DomainConverter::FromObservation(observation, type, identity, context);
		if (converted)
		{
			return {std::move(*converted), {}};
		}
		auto unclassified = observation;
		unclassified.Type = {};
		auto generic = Nato::DomainConverter::FromObservation(unclassified, std::nullopt, identity, context).value();
		if (const auto* typeId = observation.Type.GetIf<Units::Api::TypeId>())
		{
			generic.ExternalAmplifiers.NameOrType = TypeLabel(*typeId, type);
		}
		else
		{
			generic.ExternalAmplifiers.NameOrType = "Unrepresentable classification";
		}
		return {std::move(generic), ConversionDiagnostic(converted.error())};
	}
}
