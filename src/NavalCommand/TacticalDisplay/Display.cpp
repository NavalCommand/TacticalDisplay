#include <NavalCommand/TacticalDisplay/Display.h>
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <mutex>
#include <numbers>

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
		constexpr ImU32 CONTACT_COLOR = IM_COL32(245, 191, 83, 255);

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

		const char* Classification(Units::Api::Classification classification)
		{
			switch (classification)
			{
			case Units::Api::Classification::FixedWingAircraft:
				return "Fixed wing aircraft";
			case Units::Api::Classification::RotaryWingAircraft:
				return "Rotary wing aircraft";
			case Units::Api::Classification::SurfaceVessel:
				return "Surface vessel";
			case Units::Api::Classification::Submarine:
				return "Submarine";
			case Units::Api::Classification::LandVehicle:
				return "Land vehicle";
			case Units::Api::Classification::LandBuilding:
				return "Land building";
			case Units::Api::Classification::Projectile:
				return "Projectile";
			}
			return "Unknown";
		}
	}

	class Display::State
	{
	public:
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

		void Symbol(ImDrawList& drawing, ImVec2 point, const std::string& id, const std::string& name, bool contact, std::optional<double> heading)
		{
			const auto color = contact ? CONTACT_COLOR : UNIT_COLOR;
			if (contact)
			{
				drawing.AddQuad({point.x, point.y - 7}, {point.x + 7, point.y}, {point.x, point.y + 7}, {point.x - 7, point.y}, color, 2);
			}
			else
			{
				drawing.AddCircleFilled(point, 5, color);
			}
			if (heading)
			{
				drawing.AddLine(point, {point.x + static_cast<float>(std::sin(*heading) * 22), point.y - static_cast<float>(std::cos(*heading) * 22)}, color, 2);
			}
			drawing.AddText({point.x + 12, point.y - 7}, color, name.empty() ? id.c_str() : name.c_str());
			const auto mouse = ImGui::GetIO().MousePos;
			const auto distanceSquared = (mouse.x - point.x) * (mouse.x - point.x) + (mouse.y - point.y) * (mouse.y - point.y);
			if (ImGui::IsWindowHovered() && distanceSquared < 225 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				Selection = id;
				SelectedContact = contact;
			}
			if (Selection == id && SelectedContact == contact)
			{
				drawing.AddCircle(point, 12, IM_COL32(255, 255, 255, 255), 24, 2);
			}
		}

		void UncertaintyEllipse(ImDrawList& drawing, ImVec2 point, const Observations::Api::ObservedErrorEllipse& ellipse)
		{
			const double majorPixels = ellipse.MajorAxis.Value * PixelsPerDegree / METERS_PER_DEGREE;
			const double minorPixels = ellipse.MinorAxis.Value * PixelsPerDegree / METERS_PER_DEGREE;
			for (int segment = 0; segment <= 64; ++segment)
			{
				const double parameter = segment * 2 * std::numbers::pi / 64;
				const double north = majorPixels * std::cos(parameter);
				const double east = minorPixels * std::sin(parameter);
				const double orientation = ellipse.Orientation.Value;
				drawing.PathLineTo({point.x + static_cast<float>(north * std::sin(orientation) + east * std::cos(orientation)),
									point.y - static_cast<float>(north * std::cos(orientation) - east * std::sin(orientation))});
			}
			drawing.PathStroke(IM_COL32(245, 191, 83, 120), 1.0f, ImDrawFlags_Closed);
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
						Symbol(drawing,
							   Project(position.Latitude.Value, position.Longitude.Value, origin, size),
							   id.Value,
							   unit->Name,
							   false,
							   unit->Kinematics.Heading ? std::optional{unit->Kinematics.Heading->Value} : std::nullopt);
						if (auto operations = providers.Operations.GetUnitOperations(id); operations && operations->NavigationState)
						{
							auto previous = Project(position.Latitude.Value, position.Longitude.Value, origin, size);
							for (const auto& waypoint : operations->NavigationState->PlannedRoute)
							{
								auto next = Project(waypoint.TargetPosition.Latitude.Value, waypoint.TargetPosition.Longitude.Value, origin, size);
								drawing.AddLine(previous, next, UNIT_COLOR);
								previous = next;
							}
						}
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
						std::optional<double> latitude;
						std::optional<double> longitude;
						const Observations::Api::ObservedErrorEllipse* ellipse = nullptr;
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
							if (partial.ErrorEllipse)
							{
								ellipse = &*partial.ErrorEllipse;
							}
						}
						if (!latitude || !longitude)
						{
							continue;
						}
						const auto point = Project(*latitude, *longitude, origin, size);
						if (ellipse)
						{
							UncertaintyEllipse(drawing, point, *ellipse);
						}
						Symbol(drawing,
							   point,
							   id.Value,
							   observation->ObservationName,
							   true,
							   observation->Kinematics.Heading.IsKnown() ? std::optional{observation->Kinematics.Heading.Value().Value} : std::nullopt);
					}
				}
			}
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
				if (auto type = observation->Type.GetIf<Units::Api::TypeId>())
				{
					ImGui::Text("Type: %s", type->Value.c_str());
				}
				else if (auto classification = observation->Type.GetIf<Units::Api::Classification>())
				{
					ImGui::TextUnformatted(Classification(*classification));
				}
				else
				{
					ImGui::TextUnformatted("Type unknown");
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
				if (auto exact = std::get_if<Physics::Api::Position>(&observation->Kinematics.Position))
				{
					ImGui::Text("Exact: %.5f, %.5f", exact->Latitude.Value, exact->Longitude.Value);
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
				ImGui::TextWrapped("Type: %s", unit->Type.Value.c_str());
				if (auto type = providers.Types.GetUnitType(unit->Type))
				{
					ImGui::TextUnformatted(Classification(type->UnitClassification));
				}
				if (unit->Kinematics.Position)
				{
					ImGui::Text("Position: %.5f, %.5f", unit->Kinematics.Position->Latitude.Value, unit->Kinematics.Position->Longitude.Value);
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
					ImGui::TextWrapped("%s", equipment.Type.Value.c_str());
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

	Display::Display() : m_State{std::make_unique<State>()}
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
			bool perspectiveChanged = false;
			if (ImGui::BeginCombo("Perspective", m_State->Perspective.c_str()))
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
