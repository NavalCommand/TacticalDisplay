#pragma once

// Windows defines this macro with the same name as the domain time method.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

#include <NavalCommand/Environment/Api/ITimeProvider.h>
#include <NavalCommand/Factions/Api/IProvider.h>
#include <NavalCommand/Observations/Api/IInstanceProvider.h>
#include <NavalCommand/Operations/Api/IProvider.h>
#include <NavalCommand/Simulation/Api/IPauseController.h>
#include <NavalCommand/Simulation/Api/IPauseProvider.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionController.h>
#include <NavalCommand/Simulation/Api/ITimeCompressionProvider.h>
#include <NavalCommand/Units/Api/IInstanceProvider.h>
#include <NavalCommand/Units/Api/ITypeProvider.h>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace NavalCommand::TacticalDisplay
{
	struct Providers
	{
		Units::Api::IInstanceProvider& Units;
		Units::Api::ITypeProvider& Types;
		Observations::Api::IInstanceProvider& Observations;
		Operations::Api::IProvider& Operations;
		Factions::Api::IProvider& Factions;
		Environment::Api::ITimeProvider& Time;
		Simulation::Api::IPauseProvider& Pause;
		Simulation::Api::IPauseController& PauseControl;
		Simulation::Api::ITimeCompressionProvider& Compression;
		Simulation::Api::ITimeCompressionController& CompressionControl;
	};

	struct Frame
	{
		std::shared_ptr<void> Lifetime;
		std::optional<Providers> Interfaces;
		bool Stale{};
		std::string Status;
		std::string Perspective{"Omniscient"};
	};

	// Acquisition retains one coherent provider view for the whole frame.
	// Control submission must execute work away from the rendering thread.
	// Return false if admission fails. Drain submitted work before destroying
	// Display; each work item retains the provider frame for its own lifetime.
	class Display final
	{
		class State;
		std::unique_ptr<State> m_State;

	public:
		Display();
		~Display();
		Display(const Display&) = delete;
		Display& operator=(const Display&) = delete;
		int Run(std::function<Frame()> acquire,
				std::function<void(std::optional<Factions::Api::Id>)> select,
				std::function<bool(std::function<void()>)> submit,
				std::optional<std::chrono::milliseconds> automaticClose = std::nullopt);
	};
}
