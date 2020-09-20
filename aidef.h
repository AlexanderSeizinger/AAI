// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_DEF_H
#define AAI_DEF_H

#include "System/float3.h"
#include "Sim/Misc/GlobalConstants.h"
#include <vector>
#include <string>

#define AAI_VERSION aiexport_getVersion()
#define MAP_CACHE_VERSION "MAP_DATA_0_92b"
#define MAP_LEARN_VERSION "MAP_LEARN_0_91"
#define MOD_LEARN_VERSION "MOD_LEARN_0_92"
#define CONTINENT_DATA_VERSION "MOVEMENT_MAPS_0_87"

#define AILOG_PATH "log/"
#define MAP_LEARN_PATH "learn/mod/"
#define MOD_LEARN_PATH "learn/mod/"

//! Constants used within AAI
class AAIConstants
{
public:
	// The length/width of sectors (in map tiles)
	static constexpr float sectorSize = 80.0f;

	//! @todo Make this changeable via optinal mod config file
    static constexpr float energyToMetalConversionFactor = 60.0f;

	//! Minimum combat power value
	static constexpr float minCombatPower = 0.01f;
	
	//! Maximum combat power value
	static constexpr float maxCombatPower = 20.0f;

	//! Minimum initial combat power (if unit is allowed to target units of target category)
	static constexpr float minInitialCombatPower = 1.0f;

	//! Initial combat power if unit is not allowed to target units of target category
	static constexpr float noValidTargetInitialCombarPower = 0.1f;

	//! The maximum change from a single combat (attacker killes certain unit) - prevent odd statistical values from "lucky kills" (e.g. weak units gets last shot on stong one)
	static constexpr float maxCombatPowerChangeAfterSingleCombat = 0.15f;

	//! The factor applied to determine change of combat power for killer/destroyed unit type
	static constexpr float combatPowerLearningFactor = 0.02f;

	//! Minimum combat power for a unit to be considered effective against a certain target type
	static constexpr float minAntiTargetTypeCombatPower = 0.15f;

	//! Minimum combat power vs specific target type such that a group of only one unit may participate in attacks
	static constexpr float minCombatPowerForSoloAttack  = 2.5f;

	//! Minimum weapons range difference to shorter ranged attacked before combat units try to keep their distance
	static constexpr float minWeaponRangeDiffToKeepDistance = 50.0f;

	//! Minimum unused metal storage capcity befor construction of metal storage is taken into account
	static constexpr float minUnusedMetalStorageCapacityToBuildStorage = 100.0f;

	//! Minimum unused metal storage capcity befor construction of energy storage is taken into account
	static constexpr float minUnusedEnergyStorageCapacityToBuildStorage = 600.0f;

	//! Minimum averaged metal surplus before constrcution of non-resource generating units shall be assisted
	static constexpr float minMetalSurplusForConstructionAssist = 0.5f;

	//! Minimum averaged energy surplus before constrcution of non-resource generating units shall be assisted
	static constexpr float minEnergySurplusForConstructionAssist = 40.0f;

	//! Maximum distance to rally points for units to be considered to have reached it
	static constexpr float maxSquaredDistToRallyPoint = static_cast<float>( (16*SQUARE_SIZE)*(16*SQUARE_SIZE) );
};

class AAIMetalSpot
{
public:
	AAIMetalSpot(float3 _pos, float _amount):
		pos(_pos),
		occupied(false),
		extractor(-1),
		extractor_def(-1),
		amount(_amount)
	{}
	AAIMetalSpot():
		pos(ZeroVector),
		occupied(false),
		extractor(-1),
		extractor_def(-1),
		amount(0)
	{}

	float3 pos;
	bool occupied;
	int extractor;		// -1 if unocuppied
	int extractor_def;	// -1 if unocuppied
	float amount;
};

enum UnitCategory {UNKNOWN, STATIONARY_DEF, STATIONARY_ARTY, STORAGE, STATIONARY_CONSTRUCTOR, AIR_BASE,
STATIONARY_RECON, STATIONARY_JAMMER, STATIONARY_LAUNCHER, DEFLECTION_SHIELD, POWER_PLANT, EXTRACTOR, METAL_MAKER,
COMMANDER, GROUND_ASSAULT, AIR_ASSAULT, HOVER_ASSAULT, SEA_ASSAULT, SUBMARINE_ASSAULT, GROUND_ARTY, SEA_ARTY, HOVER_ARTY,
SCOUT, MOBILE_TRANSPORT, MOBILE_JAMMER, MOBILE_LAUNCHER, MOBILE_CONSTRUCTOR};

enum UnitTask {UNIT_IDLE, UNIT_ATTACKING, DEFENDING, GUARDING, MOVING, BUILDING, SCOUTING, ASSISTING, RECLAIMING, HEADING_TO_RALLYPOINT, UNIT_KILLED, ENEMY_UNIT, BOMB_TARGET};

//! @brief An id identifying a specific unit - used to prevent mixing ids referring to units and unit definitions
struct UnitId
{
public:
	UnitId(int unitId) : id(unitId) { };

	UnitId() : id(-1) { };

	bool operator==(const UnitId& rhs) const { return (id == rhs.id); }

	bool IsValid() const { return (id >= 0) ? true : false; };

	void Invalidate() { id = -1; };

	int id;
};

//! @brief This class encapsulates the determination of the current game phase (ranging from start to late game) 
//!        used to differentiate when making decisions/record learning data
class GamePhase
{
public:
	GamePhase(int frame)
	{
		for(int i = 1; i < numberOfGamePhases; ++i) // starting with i=1 on purpose 
		{
			if(frame < m_startFrameOfGamePhase[i])
			{
				m_gamePhase = i-1;
				return;
			}
		}
		m_gamePhase = 3;
	}

	bool operator>(const GamePhase& rhs) const { return (m_gamePhase > rhs.m_gamePhase); }

	bool operator<(const GamePhase& rhs) const { return (m_gamePhase < rhs.m_gamePhase); }

	bool operator<=(const GamePhase& rhs) const { return (m_gamePhase <= rhs.m_gamePhase); }

	int GetArrayIndex()          const { return m_gamePhase; };

	const std::string& GetName() const { return m_gamePhaseNames[m_gamePhase]; }

	bool IsStartingPhase()       const {return (m_gamePhase == 0); }

	bool IsEarlyPhase()          const {return (m_gamePhase == 1); }

	bool IsIntermediatePhase()   const {return (m_gamePhase == 2); }

	bool IsLatePhase()           const {return (m_gamePhase == 3); }

	void Next() { ++m_gamePhase; }

	bool End() const { return (m_gamePhase >= numberOfGamePhases); }

	static const int numberOfGamePhases = 4;

private:
	int m_gamePhase;

	//! Frame at which respective game phase starts: 0 -> 0 min, 1 -> 6min, 2 -> 15 min, 3 -> 40 min
	const static std::vector<int>         m_startFrameOfGamePhase;

	const static std::vector<std::string> m_gamePhaseNames;
	
	//const static inline std::vector<int> m_startFrameOfGamePhase = {0, 10800, 27000, 72000}; use when switching to Cpp17
	//const static inline std::vector<int> m_gamePhaseNames = {"starting phase", "early phase", "mid phase", "late game"}; use when switching to Cpp17
};

class SmoothedData
{
public:
	SmoothedData(int smoothingLength) : m_averageValue(0.0f), m_nextIndex(0) { m_values.resize(smoothingLength, 0.0f); } 

	float GetAverageValue() const { return m_averageValue; }

	void AddValue(float value) 
	{
		m_averageValue += (value - m_values[m_nextIndex]) / static_cast<float>(m_values.size());
		m_values[m_nextIndex] = value;
		++m_nextIndex;

		if(m_nextIndex >= m_values.size())
			m_nextIndex = 0;
	}

private:
	//! The values to be averaged
	std::vector<float> m_values;

	//! The current average value
	float m_averageValue;

	//! Index where next value will be added 
	int   m_nextIndex;
};

class AAIGroup;
class AAIConstructor;
struct AAIUnit
{
	int unit_id;
	int def_id;
	AAIGroup *group;
	AAIConstructor *cons;
	UnitTask status;
	int last_order;
};

#endif

