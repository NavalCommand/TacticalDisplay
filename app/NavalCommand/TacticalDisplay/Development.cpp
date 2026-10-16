#include <NavalCommand/Environment/Api/ITimeProviderMock.h>
#include <NavalCommand/Error/Api/MakeError.h>
#include <NavalCommand/Equipment/Api/IProviderMock.h>
#include <NavalCommand/Factions/Api/IProviderMock.h>
#include <NavalCommand/Observations/Api/IInstanceProviderMock.h>
#include <NavalCommand/Operations/Api/IProviderMock.h>
#include <NavalCommand/Simulation/Api/IPauseControllerMock.h>
#include <NavalCommand/Simulation/Api/IPauseProviderMock.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionControllerMock.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionProviderMock.h>
#include <NavalCommand/TacticalDisplay/Display.h>
#include <NavalCommand/Units/Api/IInstanceProviderMock.h>
#include <NavalCommand/Units/Api/ITypeProviderMock.h>
#include <atomic>
#include <future>
#include <iostream>

namespace NavalCommand::TacticalDisplay
{
	class NoOpRenderer final : public Symbology::Nato::Api::IRenderer
	{
	public:
		Symbology::Nato::Api::RenderResult Render(
			const Symbology::Nato::Api::Symbol&,
			const Symbology::Nato::Api::RenderContext& context) override
		{
			return {{context.Anchor, context.Anchor}, false};
		}
	};

	struct Fixture
	{
		testing::NiceMock<Units::Api::IInstanceProviderMock> Units;
		testing::NiceMock<Units::Api::ITypeProviderMock> Types;
		testing::NiceMock<Equipment::Api::IProviderMock> Equipment;
		testing::NiceMock<Observations::Api::IInstanceProviderMock> Observations;
		testing::NiceMock<Operations::Api::IProviderMock> Operations;
		testing::NiceMock<Factions::Api::IProviderMock> Factions;
		testing::NiceMock<Environment::Api::ITimeProviderMock> Time;
		testing::NiceMock<Simulation::Api::IPauseProviderMock> Pause;
		testing::NiceMock<Simulation::Api::IPauseControllerMock> PauseControl;
		testing::NiceMock<Simulation::Api::ITimeCompressionProviderMock> Compression;
		testing::NiceMock<Simulation::Api::ITimeCompressionControllerMock> CompressionControl;
		std::atomic<bool> Paused{};
		std::atomic<double> Factor{1};
		std::optional<Factions::Api::Id> Perspective;

		Fixture()
		{
			using testing::_;
			using testing::Return;
			ON_CALL(Units, GetUnitInstanceIds()).WillByDefault(Return(std::vector<Units::Api::Id>{{"own"}}));
			Units::Api::Instance unit{};
			unit.Identity.Value = "own";
			unit.Name = "Local provider fixture";
			unit.Type.Value = "frigate";
			unit.CurrentLoadout = {"empty"};
			unit.Faction.Value = "blue";
			unit.Kinematics.Position = Physics::Api::Position{{54}, {8}, {0}};
			unit.Kinematics.Speed = Physics::Api::MetersPerSecond{5};
			unit.Kinematics.Heading = Physics::Api::Radians{0.5};
			ON_CALL(Units, GetUnitInstance(_)).WillByDefault(Return(unit));
			Units::Api::Type type{};
			type.Id = {"frigate"};
			type.UnitSubCategory = Classification::Api::SubCategory::Frigate;
			Equipment::Api::Loadout loadout{};
			loadout.Id = {"empty"};
			type.Loadouts.push_back(std::move(loadout));
			ON_CALL(Types, GetUnitType(_)).WillByDefault(Return(type));
			ON_CALL(Factions, GetFactionIds()).WillByDefault(Return(std::vector<Factions::Api::Id>{{"blue"}, {"red"}}));
			ON_CALL(Observations, GetObservationIds()).WillByDefault(Return(std::vector<Observations::Api::Id>{{"contact"}, {"unlocalized"}}));
			ON_CALL(Observations, GetObservationInstance(_))
				.WillByDefault(
					[](const Observations::Api::Id& id)
					{
						Observations::Api::Instance observation{};
						observation.Identity = id;
						observation.ObservationName = id.Value == "contact" ? "Reported contact" : "Bearing-only / coordinates unknown";
						observation.ObserverFaction.Value = "blue";
						observation.IsDestroyed = decltype(observation.IsDestroyed)::Known(false);
						if (id.Value == "contact")
						{
							observation.ObservedFaction = decltype(observation.ObservedFaction)::Known({"red"});
							observation.Type = decltype(observation.Type)::Known<Classification::Api::Category>(
								Classification::Api::Category::SurfaceVessel);
						}
						Observations::Api::PartiallyLocalizedPosition position;
						if (id.Value == "contact")
						{
							position.Latitude = Physics::Api::Degrees{54.05};
							position.Longitude = Physics::Api::Degrees{8.1};
							position.ErrorEllipse = Observations::Api::ObservedErrorEllipse{{4000}, {1300}, {0.6}};
						}
						observation.Kinematics.Position = position;
						return Error::Api::Result<Observations::Api::Instance>{observation};
					});
			ON_CALL(Operations, GetUnitOperations(_)).WillByDefault(Return(Operations::Api::UnitState{}));
			ON_CALL(Time, GetCurrentTime()).WillByDefault(Return(Physics::Api::Seconds{42}));
			ON_CALL(Pause, IsPaused())
				.WillByDefault(
					[this]
					{
						return Error::Api::Result<bool>{Paused.load()};
					});
			ON_CALL(PauseControl, CanRequestPause()).WillByDefault(Return(true));
			ON_CALL(PauseControl, CanRequestUnpause()).WillByDefault(Return(true));
			ON_CALL(PauseControl, RequestPause(_))
				.WillByDefault(
					[this](bool paused)
					{
						Paused.store(paused);
						return Error::Api::DomainResult<void, Simulation::Api::Error>{};
					});
			ON_CALL(Compression, GetTimeCompression())
				.WillByDefault(
					[this]
					{
						return Error::Api::Result<Simulation::Api::TimeCompressionFactor>{{Factor.load()}};
					});
			ON_CALL(CompressionControl, SetTimeCompression(_))
				.WillByDefault(
					[this](Simulation::Api::TimeCompressionFactor factor)
					{
						Factor.store(factor.Value);
						return Error::Api::DomainResult<void, Simulation::Api::Error>{};
					});
		}
	};
}

int main(int argc, char** argv)
{
	using namespace NavalCommand;
	auto fixture = std::make_shared<TacticalDisplay::Fixture>();
	std::optional<std::chrono::milliseconds> duration;
	if (argc == 3 && std::string{argv[1]} == "--duration-ms")
	{
		duration = std::chrono::milliseconds{std::stoi(argv[2])};
	}
	std::vector<std::future<void>> controls;
	TacticalDisplay::NoOpRenderer renderer;
	TacticalDisplay::Display display{renderer};
	std::size_t frames = 0;
	const int result = display.Run(
		[&]
		{
			++frames;
			return TacticalDisplay::Frame{fixture,
										  TacticalDisplay::Providers{fixture->Units,
																	 fixture->Types,
												 fixture->Equipment,
																	 fixture->Observations,
																	 fixture->Operations,
																	 fixture->Factions,
																	 fixture->Time,
																	 fixture->Pause,
																	 fixture->PauseControl,
																	 fixture->Compression,
																	 fixture->CompressionControl},
										  false,
										  "SYNTHETIC LOCAL PROVIDERS / no game or network",
										  fixture->Perspective ? fixture->Perspective->Value : "Omniscient"};
		},
		[&](std::optional<Factions::Api::Id> selected)
		{
			fixture->Perspective = selected;
		},
		[&](std::function<void()> work)
		{
			controls.push_back(std::async(std::launch::async, std::move(work)));
			return true;
		},
		duration);
	for (auto& control : controls)
	{
		control.get();
	}
	std::cout << "Rendered " << frames << " frames from local Domain.Api providers.\n";
	return result == 0 && frames > 1 ? 0 : 1;
}
