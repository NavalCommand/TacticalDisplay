#include <NavalCommand/Error/Api/ErrorCondition.h>
#include <NavalCommand/Symbology/Nato/Api/DomainConverter.h>
#include <NavalCommand/TacticalDisplay/Display.h>
#include <NavalCommand/TacticalDisplay/SymbolLayout.h>
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <limits>
#include <map>
#include <mutex>
#include <numbers>
#include <string>
#include <vector>

#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM parameter, LPARAM detail);

namespace NavalCommand::TacticalDisplay
{
	namespace
	{
		constexpr double METERS_PER_DEGREE = 111320;
		constexpr double RADIANS_TO_DEGREES = 180 / std::numbers::pi;
		constexpr ImU32 GRID_COLOR = IM_COL32(37, 65, 79, 255);
		constexpr ImU32 UNIT_COLOR = IM_COL32(80, 215, 210, 255);
		namespace NatoApi = NavalCommand::Symbology::Nato::Api;

		std::string DisplayName(const std::string& fallback, const std::optional<std::string>& displayName, const std::optional<std::string>& alternativeDisplayName = std::nullopt)
		{
			std::string label = displayName && !displayName->empty() ? *displayName : fallback;
			if (alternativeDisplayName && !alternativeDisplayName->empty() && *alternativeDisplayName != label)
			{
				label += " (" + *alternativeDisplayName + ")";
			}
			return label;
		}

		LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM parameter, LPARAM detail)
		{
			if (ImGui_ImplWin32_WndProcHandler(window, message, parameter, detail))
			{
				return true;
			}
			if (message == WM_DESTROY)
			{
				PostQuitMessage(0);
				return 0;
			}
			if (message == WM_SYSCOMMAND && (parameter & 0xfff0) == SC_KEYMENU)
			{
				return 0;
			}
			return DefWindowProcW(window, message, parameter, detail);
		}

		const char* SubCategoryName(Classification::Api::SubCategory subCategory)
		{
			switch (subCategory)
			{
			case Classification::Api::SubCategory::FixedWingAircraft:
				return "Fixed wing aircraft";
			case Classification::Api::SubCategory::RotaryWingAircraft:
				return "Rotary wing aircraft";
			case Classification::Api::SubCategory::TiltRotorAircraft:
				return "Tilt rotor aircraft";
			case Classification::Api::SubCategory::CruiseMissile:
				return "Cruise missile";
			case Classification::Api::SubCategory::BallisticMissile:
				return "Ballistic missile";
			case Classification::Api::SubCategory::AircraftCarrier:
				return "Aircraft carrier";
			case Classification::Api::SubCategory::Cruiser:
				return "Cruiser";
			case Classification::Api::SubCategory::Destroyer:
				return "Destroyer";
			case Classification::Api::SubCategory::Frigate:
				return "Frigate";
			case Classification::Api::SubCategory::Corvette:
				return "Corvette";
			case Classification::Api::SubCategory::PatrolVessel:
				return "Patrol vessel";
			case Classification::Api::SubCategory::AmphibiousVessel:
				return "Amphibious vessel";
			case Classification::Api::SubCategory::AuxiliaryVessel:
				return "Auxiliary vessel";
			case Classification::Api::SubCategory::MerchantVessel:
				return "Merchant vessel";
			case Classification::Api::SubCategory::AttackSubmarine:
				return "Attack submarine";
			case Classification::Api::SubCategory::BallisticMissileSubmarine:
				return "Ballistic missile submarine";
			case Classification::Api::SubCategory::CruiseMissileSubmarine:
				return "Cruise missile submarine";
			case Classification::Api::SubCategory::HeavyweightTorpedo:
				return "Heavyweight torpedo";
			case Classification::Api::SubCategory::LightweightTorpedo:
				return "Lightweight torpedo";
			case Classification::Api::SubCategory::NavalMine:
				return "Naval mine";
			case Classification::Api::SubCategory::Noisemaker:
				return "Noisemaker";
			case Classification::Api::SubCategory::SelfPropelledAcousticDecoy:
				return "Self propelled acoustic decoy";
			case Classification::Api::SubCategory::Sonobuoy:
				return "Sonobuoy";
			case Classification::Api::SubCategory::GroundVehicle:
				return "Ground vehicle";
			case Classification::Api::SubCategory::SurfaceToAirMissileLauncher:
				return "Surface to air missile launcher";
			case Classification::Api::SubCategory::AntiAircraftGun:
				return "Anti aircraft gun";
			case Classification::Api::SubCategory::Radar:
				return "Radar";
			case Classification::Api::SubCategory::Airfield:
				return "Airfield";
			case Classification::Api::SubCategory::MilitaryInstallation:
				return "Military installation";
			case Classification::Api::SubCategory::CivilianInstallation:
				return "Civilian installation";
			case Classification::Api::SubCategory::Missile:
				return "Missile";
			case Classification::Api::SubCategory::Torpedo:
				return "Torpedo";
			case Classification::Api::SubCategory::SurfaceVessel:
				return "Surface vessel";
			case Classification::Api::SubCategory::Submarine:
				return "Submarine";
			case Classification::Api::SubCategory::Whale:
				return "Whale";
			case Classification::Api::SubCategory::Other:
				return "Other";
			}
			return "Unrecognized subcategory";
		}

		const char* CategoryName(Classification::Api::Category category)
		{
			switch (category)
			{
			case Classification::Api::Category::Aircraft:
				return "Aircraft";
			case Classification::Api::Category::Missile:
				return "Missile";
			case Classification::Api::Category::SurfaceVessel:
				return "Surface vessel";
			case Classification::Api::Category::Submarine:
				return "Submarine";
			case Classification::Api::Category::Torpedo:
				return "Torpedo";
			case Classification::Api::Category::Mine:
				return "Mine";
			case Classification::Api::Category::LandEquipment:
				return "Land equipment";
			case Classification::Api::Category::LandInstallation:
				return "Land installation";
			case Classification::Api::Category::LandUnit:
				return "Land unit";
			case Classification::Api::Category::SpaceObject:
				return "Space object";
			case Classification::Api::Category::SurfaceDevice:
				return "Surface device";
			case Classification::Api::Category::SubsurfaceDevice:
				return "Subsurface device";
			case Classification::Api::Category::SeaAnimal:
				return "Sea animal";
			case Classification::Api::Category::Other:
				return "Other";
			}
			return "Unrecognized category";
		}

		const char* DimensionName(Classification::Api::Dimension dimension)
		{
			switch (dimension)
			{
			case Classification::Api::Dimension::Air:
				return "Air";
			case Classification::Api::Dimension::Land:
				return "Land";
			case Classification::Api::Dimension::SeaSurface:
				return "Sea surface";
			case Classification::Api::Dimension::SeaSubsurface:
				return "Sea subsurface";
			case Classification::Api::Dimension::Space:
				return "Space";
			case Classification::Api::Dimension::Other:
				return "Other";
			}
			return "Unrecognized dimension";
		}

		const char* AlignmentName(Classification::Api::Alignment alignment)
		{
			switch (alignment)
			{
			case Classification::Api::Alignment::Friendly:
				return "Friendly";
			case Classification::Api::Alignment::Neutral:
				return "Neutral";
			case Classification::Api::Alignment::Hostile:
				return "Hostile";
			}
			return "Unknown";
		}

		const char* RoleName(Classification::Api::Role role)
		{
			switch (role)
			{
			case Classification::Api::Role::AirSuperiority:
				return "Air superiority";
			case Classification::Api::Role::AirDefence:
				return "Air defence";
			case Classification::Api::Role::Strike:
				return "Strike";
			case Classification::Api::Role::LandAttack:
				return "Land attack";
			case Classification::Api::Role::SurfaceWarfare:
				return "Surface warfare";
			case Classification::Api::Role::AntiSubmarineWarfare:
				return "Anti submarine warfare";
			case Classification::Api::Role::MineWarfare:
				return "Mine warfare";
			case Classification::Api::Role::Reconnaissance:
				return "Reconnaissance";
			case Classification::Api::Role::MaritimePatrol:
				return "Maritime patrol";
			case Classification::Api::Role::AirborneEarlyWarning:
				return "Airborne early warning";
			case Classification::Api::Role::ElectronicWarfare:
				return "Electronic warfare";
			case Classification::Api::Role::Transport:
				return "Transport";
			case Classification::Api::Role::AerialRefuelling:
				return "Aerial refuelling";
			case Classification::Api::Role::Surveillance:
				return "Surveillance";
			case Classification::Api::Role::Decoy:
				return "Decoy";
			}
			return "Unrecognized role";
		}

		std::string RoleList(const std::vector<Classification::Api::Role>& roles)
		{
			if (roles.empty())
			{
				return "none specified";
			}

			std::string result;
			for (const auto role : roles)
			{
				if (!result.empty())
				{
					result += ", ";
				}
				result += RoleName(role);
			}
			return result;
		}

		const char* ContextName(NatoApi::Context context)
		{
			switch (context)
			{
			case NatoApi::Context::Reality:
				return "Reality";
			case NatoApi::Context::Exercise:
				return "Exercise";
			case NatoApi::Context::Simulation:
				return "Simulation";
			}
			return "Simulation";
		}

		const char* ErrorConditionName(NavalCommand::Error::Api::ErrorCondition condition)
		{
			using ErrorCondition = NavalCommand::Error::Api::ErrorCondition;
			switch (condition)
			{
			case ErrorCondition::ContractError:
				return "ContractError";
			case ErrorCondition::CommunicationError:
				return "CommunicationError";
			case ErrorCondition::DeviceError:
				return "DeviceError";
			case ErrorCondition::NotFound:
				return "NotFound";
			case ErrorCondition::NotReady:
				return "NotReady";
			case ErrorCondition::PermissionDenied:
				return "PermissionDenied";
			case ErrorCondition::ResourceExhausted:
				return "ResourceExhausted";
			case ErrorCondition::UnknownError:
				return "UnknownError";
			}
			return "UnknownError";
		}
	}

	class Display::State
	{
	public:
		NatoApi::IRenderer& Renderer;
		NatoApi::Context SymbolContext{NatoApi::Context::Reality};
		float SymbolScale{1.0f};
		float SpeedLeaderSeconds{60.0f};
		int LabelMode{3};
		bool DetailedLabels{true};
		bool MapHovered{};
		bool MapClicked{};
		int StyleIndex{};
		SymbolLayout Layout;
		std::string LayoutScope;
		HWND Window{};
		ID3D11Device* Device{};
		ID3D11DeviceContext* Context{};
		IDXGISwapChain* SwapChain{};
		ID3D11RenderTargetView* Target{};
		double CenterLatitude{};
		double CenterLongitude{};
		bool Centered{};
		bool Win32Initialized{};
		bool RendererInitialized{};
		double PixelsPerDegree{600};
		double RequestedCompression{1};
		std::string Selection;
		bool SelectedContact{};
		std::string Perspective{"Omniscient"};
		std::mutex ControlMutex;
		std::string ControlMessage;

		explicit State(NatoApi::IRenderer& renderer) : Renderer{renderer}
		{
		}

		void ReleaseTarget()
		{
			if (Target)
			{
				Target->Release();
				Target = nullptr;
			}
		}

		bool MakeTarget()
		{
			ID3D11Texture2D* buffer = nullptr;
			if (FAILED(SwapChain->GetBuffer(0, IID_PPV_ARGS(&buffer))))
			{
				return false;
			}
			const auto status = Device->CreateRenderTargetView(buffer, nullptr, &Target);
			buffer->Release();
			return SUCCEEDED(status);
		}

		bool Initialize()
		{
			WNDCLASSW windowClass{};
			windowClass.lpfnWndProc = WindowProcedure;
			windowClass.hInstance = GetModuleHandleW(nullptr);
			windowClass.lpszClassName = L"NavalCommand.TacticalDisplay";
			windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
			RegisterClassW(&windowClass);
			Window = CreateWindowW(windowClass.lpszClassName,
								   L"NavalCommand CommandCenter",
								   WS_OVERLAPPEDWINDOW,
								   CW_USEDEFAULT,
								   CW_USEDEFAULT,
								   1440,
								   900,
								   nullptr,
								   nullptr,
								   windowClass.hInstance,
								   nullptr);
			if (!Window)
			{
				return false;
			}
			DXGI_SWAP_CHAIN_DESC description{};
			description.BufferCount = 2;
			description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			description.OutputWindow = Window;
			description.SampleDesc.Count = 1;
			description.Windowed = true;
			description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
			const auto created =
				D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &description, &SwapChain, &Device, nullptr, &Context);
			if (FAILED(created) || !MakeTarget())
			{
				return false;
			}
			ImGui::CreateContext();
			char windowsDirectory[MAX_PATH]{};
			if (GetWindowsDirectoryA(windowsDirectory, MAX_PATH))
			{
				const std::string fontPath = std::string{windowsDirectory} + "/Fonts/segoeui.ttf";
				ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f, nullptr, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
			}
			ImGui::StyleColorsDark();
			ImGui::GetStyle().WindowRounding = 0;
			ImGui::GetStyle().FrameRounding = 3;
			Win32Initialized = ImGui_ImplWin32_Init(Window);
			RendererInitialized = Win32Initialized && ImGui_ImplDX11_Init(Device, Context);
			if (!RendererInitialized)
			{
				return false;
			}
			ShowWindow(Window, SW_SHOWDEFAULT);
			return true;
		}

		~State()
		{
			if (ImGui::GetCurrentContext())
			{
				if (RendererInitialized)
				{
					ImGui_ImplDX11_Shutdown();
				}
				if (Win32Initialized)
				{
					ImGui_ImplWin32_Shutdown();
				}
				ImGui::DestroyContext();
			}
			ReleaseTarget();
			if (SwapChain)
			{
				SwapChain->Release();
			}
			if (Context)
			{
				Context->Release();
			}
			if (Device)
			{
				Device->Release();
			}
			if (Window)
			{
				DestroyWindow(Window);
			}
			UnregisterClassW(L"NavalCommand.TacticalDisplay", GetModuleHandleW(nullptr));
		}

		ImVec2 Project(double latitude, double longitude, ImVec2 origin, ImVec2 size) const
		{
			const double longitudeScale = std::max(0.05, std::cos(CenterLatitude / RADIANS_TO_DEGREES));
			const double longitudeOffset = std::remainder(longitude - CenterLongitude, 360.0);
			return {origin.x + size.x * 0.5f + static_cast<float>(longitudeOffset * PixelsPerDegree * longitudeScale),
					origin.y + size.y * 0.5f - static_cast<float>((latitude - CenterLatitude) * PixelsPerDegree)};
		}

		class MapProjection final : public NatoApi::IMapProjection
		{
			const State& Owner;
			ImVec2 Origin;
			ImVec2 Size;

		public:
			MapProjection(const State& owner, ImVec2 origin, ImVec2 size) : Owner{owner}, Origin{origin}, Size{size}
			{
			}

			NatoApi::ScreenPoint Project(NatoApi::GeographicCoordinate coordinate) const noexcept override
			{
				const ImVec2 point = Owner.Project(coordinate.LatitudeDegrees, coordinate.LongitudeDegrees, Origin, Size);
				return {point.x, point.y};
			}

			NatoApi::ScreenVector ProjectLocalOffsetMeters(NatoApi::GeographicCoordinate coordinate, double eastMeters, double northMeters) const noexcept override
			{
				const double centerLongitudeScale = std::max(0.05, std::cos(Owner.CenterLatitude / RADIANS_TO_DEGREES));
				const double localLongitudeScale = std::max(0.05, std::cos(coordinate.LatitudeDegrees / RADIANS_TO_DEGREES));
				const double pixelsPerMeter = Owner.PixelsPerDegree / METERS_PER_DEGREE;
				return {eastMeters * pixelsPerMeter * centerLongitudeScale / localLongitudeScale, -northMeters * pixelsPerMeter};
			}
		};

		void RenderScene(const std::map<SymbolKey, NatoApi::Symbol>& symbols, MapProjection& projection, ImVec2 origin, ImVec2 size)
		{
			const NatoApi::ScreenRectangle viewport{{origin.x, origin.y}, {origin.x + size.x, origin.y + size.y}};
			const auto style = NatoApi::SymbolStyle::FromPreset(static_cast<NatoApi::StylePreset>(StyleIndex));
			std::vector<LayoutItem> items;
			items.reserve(symbols.size());
			for (const auto& [key, symbol] : symbols)
			{
				const auto& location = symbol.ExternalAmplifiers.Position->Location;
				const auto anchor = projection.Project({*location.LatitudeDegrees, *location.LongitudeDegrees});
				if (viewport.Contains(anchor))
				{
					items.push_back({key, anchor, anchor, {}, Selection == key.Id && SelectedContact == key.Contact});
				}
			}
			const ImVec2 mouse = ImGui::GetIO().MousePos;
			Layout.Prepare(items, MapHovered ? std::optional<NatoApi::ScreenPoint>{{mouse.x, mouse.y}} : std::nullopt);
			const auto makeContext = [&](const LayoutItem& item)
			{
				NatoApi::RenderContext context;
				context.GeographicAnchor = item.GeographicAnchor;
				context.VisualAnchor = item.VisualAnchor;
				context.Viewport = viewport;
				context.Projection = &projection;
				context.SymbolScale = SymbolScale;
				context.SpeedLeaderTimeHorizonSeconds = SpeedLeaderSeconds;
				context.ShowLabels = LabelMode == 3 || (item.Selected && (LabelMode == 0 || LabelMode == 2)) || (Layout.IsHovered(item.Key) && (LabelMode == 1 || LabelMode == 2));
				context.DetailedLabels = DetailedLabels;
				context.Style = style;
				return context;
			};
			for (auto& item : items)
			{
				item.Geometry = Renderer.Measure(symbols.at(item.Key), makeContext(item));
			}
			std::erase_if(items,
						  [](const LayoutItem& item)
						  {
							  return !item.Geometry.Supported;
						  });
			Layout.Solve(items, viewport);

			auto& drawing = *ImGui::GetWindowDrawList();
			// Geographic graphics are all below the displaced presentation layer.
			for (const auto& item : items)
			{
				auto context = makeContext(item);
				context.Layer = NatoApi::RenderLayer::Geographic;
				Renderer.Render(symbols.at(item.Key), context);
				const auto endpoint = ConnectorEndpoint(item.GeographicAnchor, item.Geometry.SymbolBounds);
				const double horizontal = endpoint.X - item.GeographicAnchor.X;
				const double vertical = endpoint.Y - item.GeographicAnchor.Y;
				const double length = std::hypot(horizontal, vertical);
				constexpr double CONNECTOR_DASH_PIXELS = 3.0;
				constexpr double CONNECTOR_PERIOD_PIXELS = 7.0;
				if (length > 3.0)
				{
					for (double offset = 0.0; offset < length; offset += CONNECTOR_PERIOD_PIXELS)
					{
						const double end = std::min(offset + CONNECTOR_DASH_PIXELS, length);
						drawing.AddLine(
							{static_cast<float>(item.GeographicAnchor.X + horizontal * offset / length), static_cast<float>(item.GeographicAnchor.Y + vertical * offset / length)},
							{static_cast<float>(item.GeographicAnchor.X + horizontal * end / length), static_cast<float>(item.GeographicAnchor.Y + vertical * end / length)},
							IM_COL32(165, 175, 185, 180),
							1.0f);
					}
				}
			}
			// A click chooses exactly one object, including overconstrained scenes.
			if (MapClicked)
			{
				if (const auto clicked = Layout.HitTest(items, {mouse.x, mouse.y}))
				{
					Selection = clicked->Id;
					SelectedContact = clicked->Contact;
				}
			}

			for (const auto& item : items)
			{
				auto context = makeContext(item);
				context.Layer = NatoApi::RenderLayer::Presentation;
				Renderer.Render(symbols.at(item.Key), context);
				if (Selection == item.Key.Id && SelectedContact == item.Key.Contact)
				{
					const auto bounds = item.Geometry.SymbolBounds;
					drawing.AddRect({static_cast<float>(bounds.Minimum.X - 3), static_cast<float>(bounds.Minimum.Y - 3)},
									{static_cast<float>(bounds.Maximum.X + 3), static_cast<float>(bounds.Maximum.Y + 3)},
									IM_COL32(255, 255, 255, 230));
				}
			}
			// Displaced symbols expose their true position; centered symbols need no dot.
			for (const auto& item : items)
			{
				if (!HasDeclutteringOffset(item))
				{
					continue;
				}
				const auto color = style.Palette.Resolve(symbols.at(item.Key).Identification.Fields().Identity);
				const ImVec2 anchor{static_cast<float>(item.GeographicAnchor.X), static_cast<float>(item.GeographicAnchor.Y)};
				drawing.AddCircleFilled(anchor, 3.2f, IM_COL32(0, 0, 0, 255));
				drawing.AddCircleFilled(anchor, 2.2f, IM_COL32(color.Red, color.Green, color.Blue, color.Alpha));
			}
			Layout.Persist(items);
		}

		void Map(Frame& frame)
		{
			if (!Centered && frame.Interfaces)
			{
				if (auto ids = frame.Interfaces->Units.GetUnitInstanceIds())
				{
					for (const auto& id : *ids)
					{
						auto unit = frame.Interfaces->Units.GetUnitInstance(id);
						if (unit && unit->Kinematics.Position)
						{
							CenterLatitude = unit->Kinematics.Position->Latitude.Value;
							CenterLongitude = unit->Kinematics.Position->Longitude.Value;
							Centered = true;
							break;
						}
					}
				}
			}
			const auto origin = ImGui::GetCursorScreenPos();
			const auto size = ImGui::GetContentRegionAvail();
			ImGui::InvisibleButton("Geographical plot", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
			MapHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
			MapClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
			auto& drawing = *ImGui::GetWindowDrawList();
			drawing.PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
			drawing.AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(13, 30, 41, 255));
			if (ImGui::IsItemHovered())
			{
				PixelsPerDegree = std::clamp(PixelsPerDegree * std::pow(1.18, ImGui::GetIO().MouseWheel), 1.0, 200000.0);
				if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
				{
					const auto movement = ImGui::GetIO().MouseDelta;
					CenterLatitude = std::clamp(CenterLatitude + movement.y / PixelsPerDegree, -85.0, 85.0);
					CenterLongitude -= movement.x / (PixelsPerDegree * std::max(0.05, std::cos(CenterLatitude / RADIANS_TO_DEGREES)));
				}
			}
			const double gridDegrees = std::pow(10.0, std::floor(std::log10(100.0 / PixelsPerDegree)));
			const double latitudeRadius = size.y / PixelsPerDegree;
			const double longitudeRadius = size.x / (PixelsPerDegree * std::max(0.05, std::cos(CenterLatitude / RADIANS_TO_DEGREES)));
			for (double latitude = std::floor((CenterLatitude - latitudeRadius) / gridDegrees) * gridDegrees; latitude < CenterLatitude + latitudeRadius; latitude += gridDegrees)
			{
				const auto point = Project(latitude, CenterLongitude, origin, size);
				drawing.AddLine({origin.x, point.y}, {origin.x + size.x, point.y}, GRID_COLOR);
			}
			for (double longitude = std::floor((CenterLongitude - longitudeRadius) / gridDegrees) * gridDegrees; longitude < CenterLongitude + longitudeRadius;
				 longitude += gridDegrees)
			{
				const auto point = Project(CenterLatitude, longitude, origin, size);
				drawing.AddLine({point.x, origin.y}, {point.x, origin.y + size.y}, GRID_COLOR);
			}
			const std::string scope = frame.Perspective + "/" + frame.Session;
			if (LayoutScope != scope)
			{
				Layout.Clear();
				LayoutScope = scope;
			}
			MapProjection projection{*this, origin, size};
			std::map<SymbolKey, NatoApi::Symbol> symbols;
			if (frame.Interfaces)
			{
				auto& providers = *frame.Interfaces;
				if (auto ids = providers.Units.GetUnitInstanceIds())
				{
					for (const auto& id : *ids)
					{
						auto unit = providers.Units.GetUnitInstance(id);
						if (!unit || !unit->Kinematics.Position)
						{
							continue;
						}
						const auto& position = *unit->Kinematics.Position;
						if (auto operations = providers.Operations.GetUnitOperations(id); operations && operations->NavigationState)
						{
							auto previous = Project(position.Latitude.Value, position.Longitude.Value, origin, size);
							for (const auto& waypoint : operations->NavigationState->PlannedRoute)
							{
								auto next = Project(waypoint.TargetPosition.Latitude.Value, waypoint.TargetPosition.Longitude.Value, origin, size);
								drawing.AddLine(previous, next, IM_COL32(145, 145, 145, 255));
								previous = next;
							}
						}
					}
				}
				if (auto ids = providers.Units.GetUnitInstanceIds())
				{
					for (const auto& id : *ids)
					{
						auto unit = providers.Units.GetUnitInstance(id);
						if (!unit || !unit->Kinematics.Position)
						{
							continue;
						}
						std::optional<Units::Api::Type> typeDefinition;
						if (auto type = providers.Types.GetUnitType(unit->Type); type)
						{
							typeDefinition = std::move(*type);
						}
						std::optional<Classification::Api::Alignment> alignment;
						if (Perspective != "Omniscient")
						{
							if (auto result = providers.Factions.GetFactionAlignment({Perspective}, unit->Faction); result)
							{
								alignment = *result;
							}
						}
						auto symbol = NatoApi::DomainConverter::FromUnit(*unit, typeDefinition, NatoApi::DomainConverter::FromAlignment(alignment), SymbolContext);
						if (!symbol)
						{
							continue;
						}
						symbols.emplace(SymbolKey{false, id.Value}, std::move(*symbol));
					}
				}
				if (auto ids = providers.Observations.GetObservationIds())
				{
					for (const auto& id : *ids)
					{
						auto observation = providers.Observations.GetObservationInstance(id);
						if (!observation)
						{
							continue;
						}
						std::optional<Units::Api::Type> typeDefinition;
						if (const auto* typeId = observation->Type.GetIf<Units::Api::TypeId>())
						{
							if (auto type = providers.Types.GetUnitType(*typeId); type)
							{
								typeDefinition = std::move(*type);
							}
						}
						std::optional<Classification::Api::Alignment> alignment;
						if (observation->ObservedFaction.IsKnown())
						{
							if (auto result = providers.Factions.GetFactionAlignment(observation->ObserverFaction, observation->ObservedFaction.Value()); result)
							{
								alignment = *result;
							}
						}
						auto symbol = NatoApi::DomainConverter::FromObservation(*observation, typeDefinition, NatoApi::DomainConverter::FromAlignment(alignment), SymbolContext);
						if (!symbol)
						{
							continue;
						}
						std::optional<double> latitude;
						std::optional<double> longitude;
						if (auto exact = std::get_if<Physics::Api::Position>(&observation->Kinematics.Position))
						{
							latitude = exact->Latitude.Value;
							longitude = exact->Longitude.Value;
						}
						else
						{
							const auto& partial = std::get<Observations::Api::PartiallyLocalizedPosition>(observation->Kinematics.Position);
							if (partial.Latitude)
							{
								latitude = partial.Latitude->Value;
							}
							if (partial.Longitude)
							{
								longitude = partial.Longitude->Value;
							}
						}
						if (!latitude || !longitude)
						{
							continue;
						}
						symbols.emplace(SymbolKey{true, id.Value}, std::move(*symbol));
					}
				}
			}
			RenderScene(symbols, projection, origin, size);
			drawing.AddText({origin.x + 12, origin.y + size.y - 25}, IM_COL32(155, 175, 185, 255), "Right drag: pan     Wheel: zoom     Click: inspect");
			drawing.PopClipRect();
		}

		void Details(Frame& frame)
		{
			ImGui::TextUnformatted("TACTICAL OBJECTS");
			if (!frame.Interfaces)
			{
				return;
			}
			auto& providers = *frame.Interfaces;
			if (auto ids = providers.Units.GetUnitInstanceIds())
			{
				for (const auto& id : *ids)
				{
					auto unit = providers.Units.GetUnitInstance(id);
					if (unit && ImGui::Selectable(("Unit: " + unit->Name + "##" + id.Value).c_str(), Selection == id.Value && !SelectedContact))
					{
						Selection = id.Value;
						SelectedContact = false;
					}
				}
			}
			if (auto ids = providers.Observations.GetObservationIds())
			{
				for (const auto& id : *ids)
				{
					auto observation = providers.Observations.GetObservationInstance(id);
					if (observation && ImGui::Selectable(("Contact: " + observation->ObservationName + "##contact" + id.Value).c_str(), Selection == id.Value && SelectedContact))
					{
						Selection = id.Value;
						SelectedContact = true;
					}
				}
			}
			ImGui::Separator();
			if (Selection.empty())
			{
				ImGui::TextWrapped("Select a unit or contact to inspect the information available in this perspective.");
				return;
			}
			if (SelectedContact)
			{
				auto observation = providers.Observations.GetObservationInstance({Selection});
				if (!observation)
				{
					Selection.clear();
					return;
				}
				ImGui::TextWrapped("Observation: %s", observation->Identity.Value.c_str());
				std::optional<Units::Api::Type> exactUnitType;
				if (auto type = observation->Type.GetIf<Units::Api::TypeId>())
				{
					auto resolvedType = providers.Types.GetUnitType(*type);
					if (resolvedType)
					{
						exactUnitType = *resolvedType;
					}
					else
					{
						ImGui::Text("Type metadata lookup failed: %s", ErrorConditionName(resolvedType.error().Condition));
					}
					const std::string typeLabel = exactUnitType ? DisplayName(type->Value, exactUnitType->DisplayName, exactUnitType->AlternativeDisplayName) : type->Value;
					ImGui::Text("Exact type: %s", typeLabel.c_str());
					if (exactUnitType)
					{
						ImGui::Text("Primary platform role: %s", exactUnitType->PrimaryPlatformRole ? RoleName(*exactUnitType->PrimaryPlatformRole) : "Unspecified");
						ImGui::Text("Platform roles: %s", RoleList(exactUnitType->PlatformRoles).c_str());
					}
				}
				else if (auto subCategory = observation->Type.GetIf<Classification::Api::SubCategory>())
				{
					ImGui::Text("Subcategory: %s", SubCategoryName(*subCategory));
				}
				else if (auto category = observation->Type.GetIf<Classification::Api::Category>())
				{
					ImGui::Text("Category: %s", CategoryName(*category));
				}
				else if (auto dimension = observation->Type.GetIf<Classification::Api::Dimension>())
				{
					ImGui::Text("Dimension: %s", DimensionName(*dimension));
				}
				else
				{
					ImGui::TextUnformatted("Type unknown");
				}
				if (observation->ObservedFaction.IsKnown())
				{
					const auto& observedFaction = observation->ObservedFaction.Value();
					ImGui::Text("Observed faction: %s", observedFaction.Value.c_str());
					auto alignment = providers.Factions.GetFactionAlignment(observation->ObserverFaction, observedFaction);
					if (alignment)
					{
						ImGui::Text("Alignment: %s", AlignmentName(*alignment));
					}
					else
					{
						ImGui::TextUnformatted("Alignment unavailable");
					}
				}
				else
				{
					ImGui::TextUnformatted("Observed faction and alignment unknown");
				}
				if (observation->CurrentLoadout.IsKnown())
				{
					const auto& loadoutId = observation->CurrentLoadout.Value();
					std::optional<std::string> loadoutDisplayName;
					if (exactUnitType)
					{
						const auto loadout = std::find_if(exactUnitType->Loadouts.begin(),
														  exactUnitType->Loadouts.end(),
														  [&loadoutId](const auto& candidate)
														  {
															  return candidate.Id == loadoutId;
														  });
						if (loadout != exactUnitType->Loadouts.end())
						{
							loadoutDisplayName = loadout->DisplayName;
						}
					}
					const std::string loadoutLabel = DisplayName(loadoutId.Value, loadoutDisplayName);
					ImGui::Text("Selected loadout: %s", loadoutLabel.c_str());
				}
				else
				{
					ImGui::TextUnformatted("Selected loadout unknown");
				}
				if (observation->IsDestroyed.IsKnown())
				{
					ImGui::TextUnformatted(observation->IsDestroyed.Value() ? "Destroyed" : "Not destroyed");
				}
				else
				{
					ImGui::TextUnformatted("Destruction status unknown");
				}
				if (observation->Kinematics.Speed.IsKnown())
				{
					ImGui::Text("Speed: %.2f m/s", observation->Kinematics.Speed.Value().Value);
				}
				else
				{
					ImGui::TextUnformatted("Speed unknown");
				}
				if (observation->Kinematics.Heading.IsKnown())
				{
					ImGui::Text("Heading: %.1f deg", observation->Kinematics.Heading.Value().Value * RADIANS_TO_DEGREES);
				}
				else
				{
					ImGui::TextUnformatted("Heading unknown");
				}
				if (auto exact = std::get_if<Physics::Api::Position>(&observation->Kinematics.Position))
				{
					ImGui::Text("Exact: %.5f, %.5f", exact->Latitude.Value, exact->Longitude.Value);
					ImGui::Text("Elevation: %.1f m", exact->Elevation.Value);
				}
				else
				{
					const auto& partial = std::get<Observations::Api::PartiallyLocalizedPosition>(observation->Kinematics.Position);
					if (partial.Latitude)
					{
						ImGui::Text("Latitude: %.5f deg", partial.Latitude->Value);
					}
					else
					{
						ImGui::TextUnformatted("Latitude unknown; not plotted");
					}
					if (partial.Longitude)
					{
						ImGui::Text("Longitude: %.5f deg", partial.Longitude->Value);
					}
					else
					{
						ImGui::TextUnformatted("Longitude unknown; not plotted");
					}
					if (partial.Elevation)
					{
						ImGui::Text("Elevation: %.1f m", partial.Elevation->Value);
					}
					else
					{
						ImGui::TextUnformatted("Elevation unknown");
					}
					if (partial.ErrorEllipse)
					{
						ImGui::Text("Uncertainty radii: %.0f / %.0f m", partial.ErrorEllipse->MajorAxis.Value, partial.ErrorEllipse->MinorAxis.Value);
					}
				}
			}
			else
			{
				auto unit = providers.Units.GetUnitInstance({Selection});
				if (!unit)
				{
					Selection.clear();
					return;
				}
				ImGui::TextWrapped("Unit: %s", unit->Name.c_str());
				ImGui::Text("Faction: %s", unit->Faction.Value.c_str());
				auto unitType = providers.Types.GetUnitType(unit->Type);
				const std::string typeLabel = unitType ? DisplayName(unit->Type.Value, unitType->DisplayName, unitType->AlternativeDisplayName) : unit->Type.Value;
				ImGui::TextWrapped("Type: %s", typeLabel.c_str());
				if (!unitType)
				{
					ImGui::Text("Type metadata lookup failed: %s", ErrorConditionName(unitType.error().Condition));
				}
				std::optional<std::string> loadoutDisplayName;
				if (unitType)
				{
					const auto selectedLoadout = std::find_if(unitType->Loadouts.begin(),
															  unitType->Loadouts.end(),
															  [&unit](const auto& loadout)
															  {
																  return loadout.Id == unit->CurrentLoadout;
															  });
					if (selectedLoadout != unitType->Loadouts.end())
					{
						loadoutDisplayName = selectedLoadout->DisplayName;
					}
				}
				const std::string loadoutLabel = DisplayName(unit->CurrentLoadout.Value, loadoutDisplayName);
				ImGui::Text("Selected loadout: %s", loadoutLabel.c_str());
				ImGui::TextUnformatted(unit->IsDestroyed ? "Destroyed" : "Not destroyed");
				if (unitType)
				{
					ImGui::Text("Subcategory: %s", SubCategoryName(unitType->UnitSubCategory));
					ImGui::Text("Category: %s", CategoryName(Classification::Api::GetCategory(unitType->UnitSubCategory)));
					ImGui::Text("Dimension: %s", DimensionName(Classification::Api::GetDimension(Classification::Api::GetCategory(unitType->UnitSubCategory))));
					ImGui::Text("Primary platform role: %s", unitType->PrimaryPlatformRole ? RoleName(*unitType->PrimaryPlatformRole) : "Unspecified");
					ImGui::Text("Platform roles: %s", RoleList(unitType->PlatformRoles).c_str());
					const auto selectedLoadout = std::find_if(unitType->Loadouts.begin(),
															  unitType->Loadouts.end(),
															  [&unit](const auto& loadout)
															  {
																  return loadout.Id == unit->CurrentLoadout;
															  });
					if (selectedLoadout != unitType->Loadouts.end())
					{
						ImGui::Text("Selected loadout roles: %s", RoleList(selectedLoadout->Roles).c_str());
					}
				}
				if (unit->Kinematics.Position)
				{
					ImGui::Text("Position: %.5f, %.5f", unit->Kinematics.Position->Latitude.Value, unit->Kinematics.Position->Longitude.Value);
					ImGui::Text("Elevation: %.1f m", unit->Kinematics.Position->Elevation.Value);
				}
				if (unit->Kinematics.Speed)
				{
					ImGui::Text("Speed: %.2f m/s", unit->Kinematics.Speed->Value);
				}
				if (unit->Kinematics.Heading)
				{
					ImGui::Text("Heading: %.1f deg", unit->Kinematics.Heading->Value * RADIANS_TO_DEGREES);
				}
				if (unit->Aircraft && unit->Aircraft->Airborne)
				{
					ImGui::TextUnformatted(*unit->Aircraft->Airborne ? "Airborne" : "Grounded");
				}
				ImGui::SeparatorText("Equipment capabilities");
				for (const auto& equipment : unit->Equipment)
				{
					auto equipmentType = providers.Equipment.GetEquipmentType(equipment.Type);
					const std::string equipmentLabel =
						equipmentType ? DisplayName(equipment.Type.Value, equipmentType->DisplayName, equipmentType->AlternativeDisplayName) : equipment.Type.Value;
					ImGui::TextWrapped("%s", equipmentLabel.c_str());
					ImGui::Text("State: %s / %s",
								equipment.State == Equipment::Api::OperationalState::Active	 ? "active"
								: equipment.State == Equipment::Api::OperationalState::Ready ? "ready"
																							 : "unavailable",
								equipment.DamageState == Equipment::Api::Damage::Intact	   ? "intact"
								: equipment.DamageState == Equipment::Api::Damage::Damaged ? "damaged"
																						   : "destroyed");
					if (equipment.Inventory)
					{
						if (auto discrete = std::get_if<std::uint64_t>(&equipment.Inventory->Quantity))
						{
							ImGui::Text("%s: %llu", equipment.Inventory->ResourceId.Value.c_str(), static_cast<unsigned long long>(*discrete));
						}
						else
						{
							ImGui::Text("%s: %.2f", equipment.Inventory->ResourceId.Value.c_str(), std::get<double>(equipment.Inventory->Quantity));
						}
					}
				}
				if (auto operations = providers.Operations.GetUnitOperations(unit->Identity))
				{
					if (std::holds_alternative<Operations::Api::Idle>(operations->CurrentActivity))
					{
						ImGui::TextUnformatted("Activity: idle");
					}
					else if (std::holds_alternative<Operations::Api::MovingToWaypoint>(operations->CurrentActivity))
					{
						ImGui::TextUnformatted("Activity: moving to waypoint");
					}
					else if (std::holds_alternative<Operations::Api::EngagingWith>(operations->CurrentActivity))
					{
						ImGui::TextUnformatted("Activity: engaging");
					}
					else if (std::holds_alternative<Operations::Api::EvasiveManeuvers>(operations->CurrentActivity))
					{
						ImGui::TextUnformatted("Activity: evasive maneuvers");
					}
					else
					{
						ImGui::TextUnformatted("Activity unknown");
					}
					if (operations->NavigationState)
					{
						ImGui::Text("Route: %zu waypoints", operations->NavigationState->PlannedRoute.size());
					}
					if (operations->AirOperationsState && operations->AirOperationsState->OnboardCount)
					{
						ImGui::Text("Aircraft onboard: %u", *operations->AirOperationsState->OnboardCount);
					}
				}
			}
		}
	};

	Display::Display(NatoApi::IRenderer& renderer) : m_State{std::make_unique<State>(renderer)}
	{
	}

	Display::~Display() = default;

	int Display::Run(std::function<Frame()> acquire,
					 std::function<void(std::optional<Factions::Api::Id>)> select,
					 std::function<bool(std::function<void()>)> submit,
					 std::optional<std::chrono::milliseconds> automaticClose)
	{
		if (!acquire || !select || !submit || !m_State->Initialize())
		{
			return 1;
		}
		const auto started = std::chrono::steady_clock::now();
		bool done = false;
		while (!done)
		{
			MSG message;
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&message);
				DispatchMessageW(&message);
				if (message.message == WM_QUIT)
				{
					done = true;
				}
			}
			if (done || (automaticClose && std::chrono::steady_clock::now() - started >= *automaticClose))
			{
				break;
			}
			RECT client;
			GetClientRect(m_State->Window, &client);
			DXGI_SWAP_CHAIN_DESC description;
			m_State->SwapChain->GetDesc(&description);
			if (client.right > 0 && client.bottom > 0 &&
				(description.BufferDesc.Width != static_cast<unsigned>(client.right) || description.BufferDesc.Height != static_cast<unsigned>(client.bottom)))
			{
				m_State->ReleaseTarget();
				if (FAILED(m_State->SwapChain->ResizeBuffers(0, client.right, client.bottom, DXGI_FORMAT_UNKNOWN, 0)) || !m_State->MakeTarget())
				{
					return 1;
				}
			}
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			auto frame = acquire();
			m_State->Perspective = frame.Perspective;
			ImGui::SetNextWindowPos({0, 0});
			ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
			ImGui::Begin("CommandCenter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
			ImGui::TextUnformatted("NAVALCOMMAND  /  TACTICAL PICTURE");
			ImGui::SameLine();
			ImGui::TextColored(frame.Stale ? ImVec4{1, 0.65f, 0.25f, 1} : ImVec4{0.3f, 0.85f, 0.8f, 1}, "%s", frame.Status.c_str());
			ImGui::Separator();
			bool perspectiveChanged = false;
			const int controlColumns = std::clamp(static_cast<int>(ImGui::GetContentRegionAvail().x / 300.0f), 1, 3);
			if (ImGui::BeginTable("Presentation controls", controlColumns, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersInnerH))
			{
				const auto control = [](const char* label)
				{
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(label);
					ImGui::SetNextItemWidth(-std::numeric_limits<float>::min());
				};
				control("Perspective");
				if (ImGui::BeginCombo("##Perspective", m_State->Perspective.c_str()))
				{
					if (ImGui::Selectable("Omniscient", m_State->Perspective == "Omniscient"))
					{
						m_State->Perspective = "Omniscient";
						select(std::nullopt);
						perspectiveChanged = true;
					}
					if (frame.Interfaces)
					{
						if (auto factions = frame.Interfaces->Factions.GetFactionIds())
						{
							for (const auto& faction : *factions)
							{
								if (ImGui::Selectable(faction.Value.c_str(), m_State->Perspective == faction.Value))
								{
									m_State->Perspective = faction.Value;
									select(faction);
									perspectiveChanged = true;
								}
							}
						}
					}
					ImGui::EndCombo();
				}
				control("Symbol context");
				if (ImGui::BeginCombo("##Symbol context", ContextName(m_State->SymbolContext)))
				{
					for (const auto context : {NatoApi::Context::Reality, NatoApi::Context::Exercise, NatoApi::Context::Simulation})
					{
						if (ImGui::Selectable(ContextName(context), m_State->SymbolContext == context))
						{
							m_State->SymbolContext = context;
						}
					}
					ImGui::EndCombo();
				}
				control("Symbol scale");
				if (ImGui::SliderFloat("##Symbol scale", &m_State->SymbolScale, 0.5f, 4.0f, "%.1fx", ImGuiSliderFlags_AlwaysClamp))
				{
					m_State->SymbolScale = std::round(m_State->SymbolScale * 10.0f) / 10.0f;
				}
				control("Style");
				ImGui::Combo("##Style", &m_State->StyleIndex, "Filled dark\0Unfilled dark\0");
				control("Labels");
				ImGui::Combo("##Labels", &m_State->LabelMode, "Only Selected\0Only Hovered\0Selected and Hovered\0Always\0");
				control("Label detail");
				ImGui::Checkbox("All labels (off: type only)", &m_State->DetailedLabels);
				control("Speed leader (seconds)");
				ImGui::InputFloat("##Speed leader", &m_State->SpeedLeaderSeconds, 10.0f, 60.0f, "%.0f");
				if (!std::isfinite(m_State->SpeedLeaderSeconds))
				{
					m_State->SpeedLeaderSeconds = 60.0f;
				}
				m_State->SpeedLeaderSeconds = std::clamp(m_State->SpeedLeaderSeconds, 0.0f, 86400.0f);
				ImGui::EndTable();
			}
			ImGui::Separator();
			if (perspectiveChanged)
			{
				m_State->Selection.clear();
				m_State->Centered = false;
				frame.Interfaces.reset();
			}
			if (frame.Interfaces)
			{
				auto& providers = *frame.Interfaces;
				if (auto time = providers.Time.GetCurrentTime())
				{
					ImGui::Text("Simulation: %.1f s", time->Value);
				}
				ImGui::SameLine();
				if (auto paused = providers.Pause.IsPaused())
				{
					ImGui::TextUnformatted(*paused ? "PAUSED" : "RUNNING");
				}
				ImGui::SameLine();
				if (auto compression = providers.Compression.GetTimeCompression())
				{
					ImGui::Text("Compression: %.2fx", compression->Value);
				}
				ImGui::BeginDisabled(frame.Stale);
				for (bool requestPause : {true, false})
				{
					if (ImGui::Button(requestPause ? "Request pause" : "Release client pause"))
					{
						const bool accepted = submit(
							[state = m_State.get(), lifetime = frame.Lifetime, controller = &providers.PauseControl, requestPause]
							{
								auto result = controller->RequestPause(requestPause);
								std::lock_guard lock{state->ControlMutex};
								state->ControlMessage = result ? "Pause request applied" : "Pause request failed";
							});
						if (!accepted)
						{
							std::lock_guard lock{m_State->ControlMutex};
							m_State->ControlMessage = "Control queue full or stopping";
						}
					}
					ImGui::SameLine();
				}
				ImGui::SetNextItemWidth(120);
				ImGui::InputDouble("Factor", &m_State->RequestedCompression, 0, 0, "%.2f");
				ImGui::SameLine();
				if (ImGui::Button("Apply compression"))
				{
					const bool accepted = submit(
						[state = m_State.get(), lifetime = frame.Lifetime, controller = &providers.CompressionControl, factor = m_State->RequestedCompression]
						{
							auto result = controller->SetTimeCompression({factor});
							std::lock_guard lock{state->ControlMutex};
							state->ControlMessage = result ? "Compression applied" : "Compression rejected or unavailable";
						});
					if (!accepted)
					{
						std::lock_guard lock{m_State->ControlMutex};
						m_State->ControlMessage = "Control queue full or stopping";
					}
				}
				ImGui::EndDisabled();
			}
			{
				std::lock_guard lock{m_State->ControlMutex};
				ImGui::TextUnformatted(m_State->ControlMessage.c_str());
			}
			ImGui::Separator();
			ImGui::BeginChild("Details", {330, 0}, true);
			m_State->Details(frame);
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("Plot", {0, 0}, true);
			m_State->Map(frame);
			ImGui::EndChild();
			ImGui::End();
			ImGui::Render();
			constexpr float CLEAR_COLOR[4]{0.04f, 0.08f, 0.11f, 1};
			m_State->Context->OMSetRenderTargets(1, &m_State->Target, nullptr);
			m_State->Context->ClearRenderTargetView(m_State->Target, CLEAR_COLOR);
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			if (FAILED(m_State->SwapChain->Present(1, 0)))
			{
				return 1;
			}
		}
		return 0;
	}
}
