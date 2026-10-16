#include <NavalCommand/Symbology/Nato/ImGui/Renderer.h>
#include <NavalCommand/TacticalDisplay/SymbolLayout.h>
#include <NavalCommand/TacticalDisplay/SymbolPresentation.h>
#include <gtest/gtest.h>
#include <imgui.h>
#include <vector>

namespace NavalCommand::TacticalDisplay
{
	namespace Nato = Symbology::Nato::Api;

	TEST(SymbolPresentationTest, UnitFallbackRetainsFactsAndDiagnosesInvalidDataAndRoles)
	{
		using Classification::Api::SubCategory;
		Units::Api::Instance unit{};
		unit.Identity.Value = "unit";
		unit.Type.Value = "support";
		unit.Name = "Support vessel";
		unit.Kinematics.Position = Physics::Api::Position{{60}, {4}, {0}};
		Units::Api::Type type{};
		type.Id = unit.Type;
		type.DisplayName = "Support type";
		for (auto subcategory : {SubCategory::Other, static_cast<SubCategory>(999), SubCategory::Destroyer})
		{
			type.UnitSubCategory = subcategory;
			type.PrimaryPlatformRole = Classification::Api::Role::AerialRefuelling;
			const auto presentation = SymbolPresentation::FromUnit(unit, type, Nato::StandardIdentity::Friendly, Nato::Context::Exercise);
			EXPECT_FALSE(presentation.Diagnostic.empty());
			const auto& fields = presentation.Value.Identification.Fields();
			EXPECT_EQ(fields.SymbolSetValue, Nato::SymbolSet::Unknown);
			EXPECT_EQ(fields.Identity, Nato::StandardIdentity::Friendly);
			EXPECT_EQ(fields.ContextValue, Nato::Context::Exercise);
			EXPECT_EQ(fields.Entity, 0);
			EXPECT_EQ(fields.Modifier1, 0);
			EXPECT_EQ(presentation.Value.ExternalAmplifiers.UniqueDesignation, unit.Name);
			EXPECT_EQ(presentation.Value.ExternalAmplifiers.NameOrType, "Support type");
			EXPECT_EQ(presentation.Value.ExternalAmplifiers.Position->Location.LatitudeDegrees, 60);
		}
	}

	TEST(SymbolPresentationTest, ObservedFallbackNeverIncreasesPrecisionOrInventsPosition)
	{
		Observations::Api::Instance observation{};
		observation.ObservationName = "Contact";
		Observations::Api::PartiallyLocalizedPosition position;
		position.Latitude = Physics::Api::Degrees{60};
		observation.Kinematics.Position = position;
		const auto check = [&]()
		{
			const auto presentation = SymbolPresentation::FromObservation(observation, std::nullopt, Nato::StandardIdentity::Hostile, Nato::Context::Reality);
			EXPECT_FALSE(presentation.Diagnostic.empty());
			EXPECT_EQ(presentation.Value.Identification.Fields().SymbolSetValue, Nato::SymbolSet::Unknown);
			EXPECT_EQ(presentation.Value.Identification.Fields().Entity, 0);
			EXPECT_EQ(presentation.Value.ExternalAmplifiers.UniqueDesignation, "Contact");
			ASSERT_TRUE(presentation.Value.ExternalAmplifiers.Position);
			EXPECT_EQ(presentation.Value.ExternalAmplifiers.Position->Location.LatitudeDegrees, 60);
			EXPECT_FALSE(presentation.Value.ExternalAmplifiers.Position->Location.LongitudeDegrees);
		};
		observation.Type = decltype(observation.Type)::Known<Classification::Api::SubCategory>(Classification::Api::SubCategory::Other);
		check();
		observation.Type = decltype(observation.Type)::Known<Classification::Api::Category>(Classification::Api::Category::Other);
		check();
		observation.Type = decltype(observation.Type)::Known<Classification::Api::Dimension>(Classification::Api::Dimension::Other);
		check();
		observation.Type = decltype(observation.Type)::Known<Classification::Api::SubCategory>(static_cast<Classification::Api::SubCategory>(999));
		check();
		observation.Type = {};
		const auto unknown = SymbolPresentation::FromObservation(observation, std::nullopt, Nato::StandardIdentity::Unknown, Nato::Context::Reality);
		EXPECT_TRUE(unknown.Diagnostic.empty());
		EXPECT_EQ(unknown.Value.Identification.Fields().SymbolSetValue, Nato::SymbolSet::Unknown);
	}

	TEST(SymbolPresentationTest, IdentifiedObservationFallbackPreservesAuthorizedTypeAndErrorReason)
	{
		Observations::Api::Instance observation{};
		Units::Api::Type type{};
		type.Id.Value = "known";
		type.DisplayName = "Known type";
		type.AlternativeDisplayName = "Alternative";
		type.UnitSubCategory = Classification::Api::SubCategory::Destroyer;
		type.PrimaryPlatformRole = Classification::Api::Role::AerialRefuelling;
		observation.Type = decltype(observation.Type)::Known<Units::Api::TypeId>(type.Id);
		observation.Kinematics.Position = Physics::Api::Position{{60}, {4}, {0}};
		const auto roleFallback = SymbolPresentation::FromObservation(observation, type, Nato::StandardIdentity::Hostile, Nato::Context::Reality);
		EXPECT_EQ(roleFallback.Value.ExternalAmplifiers.NameOrType, "Known type (Alternative)");
		EXPECT_TRUE(roleFallback.Diagnostic.starts_with("Unrepresentable classification or primary role"));
		EXPECT_EQ(roleFallback.Value.Identification.Fields().Modifier1, 0);
		type.UnitSubCategory = static_cast<Classification::Api::SubCategory>(999);
		const auto invalid = SymbolPresentation::FromObservation(observation, type, Nato::StandardIdentity::Hostile, Nato::Context::Reality);
		EXPECT_TRUE(invalid.Diagnostic.starts_with("Invalid domain value"));
		EXPECT_EQ(invalid.Value.ExternalAmplifiers.Position->Location.LongitudeDegrees, 4);
	}

	TEST(SymbolPresentationTest, FallbacksUseNormalRenderingInteractionAndStickyLayout)
	{
		ImGui::CreateContext();
		auto& io = ImGui::GetIO();
		io.DisplaySize = {1200, 800};
		io.DeltaTime = 1.0f / 60.0f;
		unsigned char* pixels{};
		int width{}, height{};
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		ImGui::NewFrame();
		ImGui::Begin("fallback");
		Symbology::Nato::ImGui::Renderer renderer;
		Units::Api::Instance unit{};
		Units::Api::Type type{};
		type.Id = unit.Type;
		type.UnitSubCategory = Classification::Api::SubCategory::Other;
		auto generic = SymbolPresentation::FromUnit(unit, type, Nato::StandardIdentity::Hostile, Nato::Context::Reality);
		// A valid standard entity whose icon is outside this renderer's subset.
		Nato::SidcFields fields;
		fields.SymbolSetValue = Nato::SymbolSet::SeaSubsurface;
		fields.Entity = 11;
		fields.EntityType = 2;
		SymbolPresentation unsupported{{Nato::Sidc::Create(fields).value(), {}, std::nullopt}, {}};
		Nato::RenderContext context;
		context.Viewport = {{0, 0}, {1200, 800}};
		context.GeographicAnchor = {500, 400};
		context.VisualAnchor = context.GeographicAnchor;
		std::vector<LayoutItem> items{{{false, "unit"}, {500, 400}, {500, 400}}, {{true, "contact"}, {500, 400}, {500, 400}}};
		SymbolLayout layout;
		layout.Prepare(items, std::nullopt);
		items[0].Geometry = generic.Measure(renderer, context);
		items[1].Geometry = unsupported.Measure(renderer, context);
		EXPECT_FALSE(unsupported.Diagnostic.empty());
		for (const auto& item : items)
		{
			EXPECT_TRUE(item.Geometry.Supported);
		}
		layout.Solve(items, context.Viewport, DeclutteringMode::Sticky);
		layout.Persist(items);
		const auto initial = items;
		for (std::size_t index = 0; index < items.size(); ++index)
		{
			context.VisualAnchor = items[index].VisualAnchor;
			EXPECT_TRUE(renderer.Render(index == 0 ? generic.Value : unsupported.Value, context).WasDrawn);
			EXPECT_EQ(layout.HitTest(items, items[index].VisualAnchor), items[index].Key);
		}
		layout.Prepare(items, items[0].VisualAnchor);
		layout.Solve(items, context.Viewport, DeclutteringMode::Sticky);
		for (std::size_t index = 0; index < items.size(); ++index)
		{
			EXPECT_DOUBLE_EQ(items[index].VisualAnchor.X, initial[index].VisualAnchor.X);
			EXPECT_DOUBLE_EQ(items[index].VisualAnchor.Y, initial[index].VisualAnchor.Y);
		}
		ImGui::End();
		ImGui::EndFrame();
		ImGui::DestroyContext();
	}
}
