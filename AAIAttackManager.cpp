// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIAttackManager.h"
#include "AAI.h"
#include "AAIBrain.h"
#include "AAIAttack.h"
#include "AAIConfig.h"
#include "AAIGroup.h"
#include "AAIMap.h"
#include "AAISector.h"

AAIAttackManager::AAIAttackManager(AAI *ai)
{
	this->ai = ai;

	m_activeAttacks.resize(cfg->MAX_ATTACKS, nullptr);
}

AAIAttackManager::~AAIAttackManager(void)
{
	for(auto attack = m_activeAttacks.begin(); attack != m_activeAttacks.end(); ++attack)
	{
		if(*attack)
			delete (*attack);
	}

	m_activeAttacks.clear();
}

void AAIAttackManager::Update()
{
	int availableAttackId(-1);

	for(int attackId = 0; attackId < m_activeAttacks.size(); ++attackId)
	{
		AAIAttack* attack = m_activeAttacks[attackId];

		if(attack)
		{
			// drop failed attacks
			if( AbortAttackIfFailed(attack) )
				availableAttackId = attackId;
			// check if sector cleared
			else if( attack->HasTargetBeenCleared() )
				AttackNextSectorOrAbort(attack);
		}
		else
			availableAttackId = attackId;
	}

	// at least one attack id is available -> check if new attack should be launched
	if(availableAttackId >= 0)
		TryToLaunchAttack(availableAttackId);
}

void AAIAttackManager::TryToLaunchAttack(int availableAttackId)
{
	//////////////////////////////////////////////////////////////////////////////////////////////
	// get all available combat/aa/arty groups for attack
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	const int numberOfContinents( AAIMap::GetNumberOfContinents() );
	std::vector< std::list<AAIGroup*> > availableAssaultGroupsOnContinent(numberOfContinents);
	std::vector< std::list<AAIGroup*> > availableAAGroupsOnContinent(numberOfContinents);

	std::list<AAIGroup*> availableAssaultGroupsGlobal;
	std::list<AAIGroup*> availableAAGroupsGlobal;

	const int numberOfAssaultUnitGroups = DetermineCombatUnitGroupsAvailableForattack(availableAssaultGroupsGlobal, availableAAGroupsGlobal,
																				availableAssaultGroupsOnContinent, availableAAGroupsOnContinent);

	// stop planning an attack if there are no combat groups available at the moment
	if(numberOfAssaultUnitGroups == 0)
		return;

	//////////////////////////////////////////////////////////////////////////////////////////////
	// calculate max attack power vs the different target types for each continent
	//////////////////////////////////////////////////////////////////////////////////////////////

	std::vector< std::vector<float> > combatPowerOnContinent(numberOfContinents, std::vector<float>(AAITargetType::numberOfTargetTypes, 0.0f) );
	std::vector<float>                combatPowerGlobal(AAITargetType::numberOfTargetTypes, 0.0f);
	MobileTargetTypeValues     numberOfAssaultGroupsOfTargetType;

	DetermineCombatPowerOfGroups(availableAssaultGroupsGlobal, combatPowerGlobal, numberOfAssaultGroupsOfTargetType);

	for(size_t continent = 0; continent < availableAssaultGroupsOnContinent.size(); ++continent)
		DetermineCombatPowerOfGroups(availableAssaultGroupsOnContinent[continent], combatPowerOnContinent[continent], numberOfAssaultGroupsOfTargetType);

	//////////////////////////////////////////////////////////////////////////////////////////////
	// determine attack sector
	//////////////////////////////////////////////////////////////////////////////////////////////

	const AAISector* targetSector = ai->Map()->DetermineSectorToAttack(combatPowerGlobal, combatPowerOnContinent, numberOfAssaultGroupsOfTargetType);			

	//////////////////////////////////////////////////////////////////////////////////////////////
	// order attack
	//////////////////////////////////////////////////////////////////////////////////////////////

	if(targetSector)
	{
		AAIAttack *attack = new AAIAttack(ai);
		m_activeAttacks[availableAttackId] = attack;

		// add combat unit groups
		AddGroupsToAttack(attack, availableAssaultGroupsOnContinent[targetSector->GetContinentID()]);
		AddGroupsToAttack(attack, availableAssaultGroupsGlobal);

		// add anti air units if necessary
		if(    (ai->Brain()->m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(ETargetType::AIR) > 0.2f)
			|| (ai->Brain()->GetRecentAttacksBy(ETargetType::AIR) > 0.9f) )
		{
			std::list<AAIGroup*> antiAirGroups;
			SelectNumberOfGroups(antiAirGroups, 1, availableAAGroupsOnContinent[targetSector->GetContinentID()], availableAAGroupsGlobal);

			AddGroupsToAttack(attack, antiAirGroups);
		}
		
		// start the attack
		attack->AttackSector(targetSector);
	}
}

void AAIAttackManager::SelectNumberOfGroups(std::list<AAIGroup*> selectedGroupList, int maxNumberOfGroups, std::list<AAIGroup*> groupList1, std::list<AAIGroup*> groupList2)
{
	int numberOfSelectedGroups(0);

	for(auto group = groupList1.begin(); group != groupList1.end(); ++group)
	{
		if(numberOfSelectedGroups >= maxNumberOfGroups)
			break;

		selectedGroupList.push_back(*group);
		++numberOfSelectedGroups;
	}

	for(auto group = groupList2.begin(); group != groupList2.end(); ++group)
	{
		if(numberOfSelectedGroups >= maxNumberOfGroups)
			break;

		selectedGroupList.push_back(*group);
		++numberOfSelectedGroups;
	}
}

void AAIAttackManager::AddGroupsToAttack(AAIAttack* attack, const std::list<AAIGroup*>& groupList) const
{
	for(auto group = groupList.begin(); group != groupList.end(); ++group)
	{
		if(attack->AddGroup(*group))
			(*group)->attack = attack; 
	}
}

int AAIAttackManager::DetermineCombatUnitGroupsAvailableForattack(  std::list<AAIGroup*>&                availableAssaultGroupsGlobal,
																	std::list<AAIGroup*>&                availableAAGroupsGlobal,
																	std::vector< std::list<AAIGroup*> >& availableAssaultGroupsOnContinent,
																	std::vector< std::list<AAIGroup*> >& availableAAGroupsOnContinent) const
{
	const std::vector<AAIUnitCategory> combatUnitCategories = { AAIUnitCategory(EUnitCategory::GROUND_COMBAT), 
															AAIUnitCategory(EUnitCategory::HOVER_COMBAT), 
															AAIUnitCategory(EUnitCategory::SEA_COMBAT),
															AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT) };

	int numberOfAssaultUnitGroups(0);

	for(auto category = combatUnitCategories.begin(); category != combatUnitCategories.end(); ++category)
	{
		for(auto group = ai->GetUnitGroupsList(*category).begin(); group != ai->GetUnitGroupsList(*category).end(); ++group)
		{
			if( (*group)->IsAvailableForAttack() )
			{
				const AAIUnitType& unitType = (*group)->GetUnitTypeOfGroup();
				if(unitType.IsAssaultUnit())
				{
					if( (*group)->GetMovementType().CannotMoveToOtherContinents() )
						availableAssaultGroupsOnContinent[(*group)->GetContinentId()].push_back(*group);
					else
						availableAssaultGroupsGlobal.push_back(*group);
						
					++numberOfAssaultUnitGroups;
				}
				else if(unitType.IsAntiAir())
				{
					if( (*group)->GetMovementType().CannotMoveToOtherContinents() )
						availableAAGroupsOnContinent[(*group)->GetContinentId()].push_back(*group);
					else
						availableAAGroupsGlobal.push_back(*group);
				}	
			}
		}
	}

	return numberOfAssaultUnitGroups;
}

void AAIAttackManager::DetermineCombatPowerOfGroups(const std::list<AAIGroup*>& groups, std::vector<float>& combatPower, MobileTargetTypeValues& numberOfGroupsOfTargetType) const
{
	for(auto group = groups.begin(); group != groups.end(); ++group)
	{
		numberOfGroupsOfTargetType.AddValueForTargetType((*group)->GetTargetType(), 1.0f);

		combatPower[AAITargetType::staticIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::STATIC);

		const AAIUnitCategory& category = (*group)->GetUnitCategoryOfGroup();

		if(category.IsGroundCombat())
			combatPower[AAITargetType::surfaceIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::SURFACE);
		else if(category.IsHoverCombat())
		{
			combatPower[AAITargetType::surfaceIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::SURFACE);
			combatPower[AAITargetType::floaterIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::FLOATER);
		}
		else if(category.IsSeaCombat() || category.IsSubmarineCombat())
		{
			combatPower[AAITargetType::floaterIndex]   += (*group)->GetCombatPowerVsTargetType(ETargetType::FLOATER);
			combatPower[AAITargetType::submergedIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::SUBMERGED);
		}
	}
}

void AAIAttackManager::AbortAttack(AAIAttack* attack)
{
	attack->StopAttack();

	for(auto a = m_activeAttacks.begin(); a != m_activeAttacks.end(); ++a)
	{
		if(*a == attack)
		{
			*a = nullptr;
			break;
		}
	}

	delete attack;
}

bool AAIAttackManager::AbortAttackIfFailed(AAIAttack *attack)
{
	if((ai->GetAICallback()->GetCurrentFrame() - attack->m_lastAttackOrderInFrame) < 30) 	// prevent command overflow
		return false;
	else if(attack->CheckIfFailed())
	{
		AbortAttack(attack);
		return true;
	}
	else
		return false;
}

void AAIAttackManager::AttackNextSectorOrAbort(AAIAttack* attack)
{
	// prevent command overflow
	if((ai->GetAICallback()->GetCurrentFrame() - attack->m_lastAttackOrderInFrame) < 60)
		return;

	// get new target sector
	const AAISector *dest = attack->DetermineSectorToContinueAttack();

	if(dest)
		attack->AttackSector(dest);
	else
		AbortAttack(attack);
}
