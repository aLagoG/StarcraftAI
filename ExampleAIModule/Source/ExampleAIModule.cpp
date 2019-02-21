#include "ExampleAIModule.h"
#include <iostream>
#include <sstream>
#include <vector>

using namespace BWAPI;
using namespace Filter;

#define PROB(x) (rand() < (RAND_MAX * x))
#define RAND(max) floor(rand() / float(RAND_MAX) * max)

bool isVespeneGas(Unit unit) {
    return unit->getType() == UnitTypes::Resource_Vespene_Geyser && unit->getResources() > 0;
}

UnitFilter isVespeneGeyser(isVespeneGas);

void ExampleAIModule::onStart() {
    // Hello World!
    Broodwar->sendText("Hello world!");

    // Print the map name.
    // BWAPI returns std::string when retrieving a string, don't forget to add
    // .c_str() when printing!
    Broodwar << "The map is " << Broodwar->mapName() << "!" << std::endl;

    // Enable the UserInput flag, which allows us to control the bot and type
    // messages.
    Broodwar->enableFlag(Flag::UserInput);

    // Uncomment the following line and the bot will know about everything
    // through the fog of war (cheat).
    // Broodwar->enableFlag(Flag::CompleteMapInformation);

    // Set the command optimization level so that common commands can be grouped
    // and reduce the bot's APM (Actions Per Minute).
    Broodwar->setCommandOptimizationLevel(2);

    // Check if this is a replay
    if (Broodwar->isReplay()) {
        // Announce the players in the replay
        Broodwar << "The following players are in this replay:" << std::endl;

        // Iterate all the players in the game using a std:: iterator
        Playerset players = Broodwar->getPlayers();
        for (auto p : players) {
            // Only print the player if they are not an observer
            if (!p->isObserver())
                Broodwar << p->getName() << ", playing as " << p->getRace() << std::endl;
        }

    } else  // if this is not a replay
    {
        // Retrieve you and your enemy's races. enemy() will just return the
        // first enemy. If you wish to deal with multiple enemies then you must
        // use enemies().
        if (Broodwar->enemy())  // First make sure there is an enemy
            Broodwar << "The matchup is " << Broodwar->self()->getRace() << " vs " << Broodwar->enemy()->getRace()
                     << std::endl;
    }
}

void ExampleAIModule::onEnd(bool isWinner) {
    // Called when the game ends
    if (isWinner) {
        // Log your win here!
    }
}

bool AttackLayer(Unit unit) {
    if (unit->isAttacking()) {
        return true;
    }
    Unit enemy = unit->getClosestUnit(IsEnemy);
    if (enemy && unit->canAttack(enemy)) {
        bool res = unit->attack(enemy);
        if (res) {
            Broodwar->sendText("Atack");
        }
        return res;
    }
    return false;
}

bool ConstructLayer(Unit unit) {
    if (unit->isIdle() && PROB(0.05f)) {
        UnitType supplyProviderType = unit->getType().getRace().getSupplyProvider();
        TilePosition targetBuildLocation = Broodwar->getBuildLocation(supplyProviderType, unit->getTilePosition());
        if (targetBuildLocation) {
            bool res = unit->build(supplyProviderType, targetBuildLocation);
            if (res) {
                Broodwar->sendText("Build Depot");
            }
            return res;
        }
    }
    return false;
}

bool DropLayer(Unit unit) {
    if (unit->isIdle() && (unit->isCarryingGas() || unit->isCarryingMinerals())) {
        bool res = unit->returnCargo();
        if (res) {
            Broodwar->sendText("Drop Resource");
        }
        return res;
    }
    return false;
}

bool GatherGas(Unit worker) {
    Unit resource = worker->getClosestUnit(IsRefinery);
    if (resource) {
        bool res = worker->gather(resource);
        if (res) {
            Broodwar->sendText("Gather Gas");
        }
        return res;
    }
    return false;
}

bool GatherMineral(Unit worker) {
    Unit resource = worker->getClosestUnit(IsMineralField);
    if (resource) {
        bool res = worker->gather(resource);
        if (res) {
            Broodwar->sendText("Gather Mineral");
        }
        return res;
    }
    return false;
}

bool GatherResourceLayer(Unit unit) {
    if (unit->isGatheringMinerals() || unit->isGatheringGas()) {
        return true;
    }
    auto first = GatherMineral;
    auto second = GatherGas;
    if (PROB(0.20)) {
        first = GatherGas;
        second = GatherMineral;
    }
	if(first(unit)){
        return true;
	} 
	return second(unit);
}

bool ConstructRefineryIfAbleToLayer(Unit worker) {
    Unit closest = worker->getClosestUnit(isVespeneGeyser);
    if (!closest) {
        return false;
    }
    bool res = worker->build(worker->getType().getRace().getRefinery(), closest->getTilePosition());
    if (res) {
        Broodwar->sendText("Building Refinery");
    }
    return res;
}

bool ExploreLayer(Unit worker) {
    if (worker->isIdle()) {
        bool res = worker->move(Position(TilePosition(RAND(Broodwar->mapWidth()), RAND(Broodwar->mapHeight()))).makeValid());
        if (res) {
            Broodwar->sendText("Exploring");
        }
        return res;
    }
    return false;
}

std::vector<bool (*)(Unit unit)> layers = {
    AttackLayer, ConstructLayer, DropLayer, ConstructRefineryIfAbleToLayer, GatherResourceLayer, ExploreLayer};

void brooksArchitecture(Unit worker) {
    for (auto layer : layers) {
        if (layer(worker)) {
            return;
        }
    }
}

void ExampleAIModule::onFrame() {
    // Called once every game frame

    // Display the game frame rate as text in the upper left area of the screen
    Broodwar->drawTextScreen(200, 0, "FPS: %d", Broodwar->getFPS());
    Broodwar->drawTextScreen(200, 20, "Average FPS: %f", Broodwar->getAverageFPS());

    // Return if the game is a replay or is paused
    if (Broodwar->isReplay() || Broodwar->isPaused() || !Broodwar->self())
        return;

    // Prevent spamming by only running our onFrame once every number of latency
    // frames. Latency frames are the number of frames before commands are
    // processed.
    if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
        return;

    // Iterate through all the units that we own
    for (auto& u : Broodwar->self()->getUnits()) {
        // Ignore the unit if it no longer exists
        // Make sure to include this block when handling any Unit pointer!
        if (!u->exists())
            continue;

        // Ignore the unit if it has one of the following status ailments
        if (u->isLockedDown() || u->isMaelstrommed() || u->isStasised())
            continue;

        // Ignore the unit if it is in one of the following states
        if (u->isLoaded() || !u->isPowered() || u->isStuck())
            continue;

        // Ignore the unit if it is incomplete or busy constructing
        if (!u->isCompleted() || u->isConstructing())
            continue;

        // Finally make the unit do some stuff!

        // If the unit is a worker unit
        if (u->getType().isWorker()) {
            brooksArchitecture(u);
        } else if (u->getType().isResourceDepot())  // A resource depot is a Command
                                                    // Center, Nexus, or Hatchery
        {
            // Order the depot to construct more workers! But only when it is
            // idle.
            if (u->isIdle() && !u->train(u->getType().getRace().getWorker())) {
                // If that fails, draw the error at the location so that you can
                // visibly see what went wrong! However, drawing the error once
                // will only appear for a single frame so create an event that
                // keeps it on the screen for some frames

                Position pos = u->getPosition();
                Error lastErr = Broodwar->getLastError();
                Broodwar->registerEvent(
                    [pos, lastErr](Game*) {
                        Broodwar->drawTextMap(pos, "%c%s", Text::White, lastErr.c_str());
                    },                              // action
                    nullptr,                        // condition
                    Broodwar->getLatencyFrames());  // frames to run

                // Retrieve the supply provider type in the case that we have
                // run out of supplies
                UnitType supplyProviderType = u->getType().getRace().getSupplyProvider();
                static int lastChecked = 0;

                // If we are supply blocked and haven't tried constructing more
                // recently
                if (lastErr == Errors::Insufficient_Supply && lastChecked + 400 < Broodwar->getFrameCount() &&
                    Broodwar->self()->incompleteUnitCount(supplyProviderType) == 0) {
                    lastChecked = Broodwar->getFrameCount();

					// Reached max supply
					if(Broodwar->self()->supplyTotal() >= 400){
                        continue;
					}

                    // Retrieve a unit that is capable of constructing the
                    // supply needed
                    Unit supplyBuilder = u->getClosestUnit(GetType == supplyProviderType.whatBuilds().first &&
                                                           (IsIdle || IsGatheringMinerals) && IsOwned);
                    // If a unit was found
                    if (supplyBuilder) {
                        if (supplyProviderType.isBuilding()) {
                            TilePosition targetBuildLocation =
                                Broodwar->getBuildLocation(supplyProviderType, supplyBuilder->getTilePosition());
                            if (targetBuildLocation) {
                                // Register an event that draws the target build
                                // location
                                Broodwar->registerEvent(
                                    [targetBuildLocation, supplyProviderType](Game*) {
                                        Broodwar->drawBoxMap(
                                            Position(targetBuildLocation),
                                            Position(targetBuildLocation + supplyProviderType.tileSize()),
                                            Colors::Blue);
                                    },
                                    nullptr,                                // condition
                                    supplyProviderType.buildTime() + 100);  // frames to run

                                // Order the builder to construct the supply
                                // structure
                                supplyBuilder->build(supplyProviderType, targetBuildLocation);
                            }
                        } else {
                            // Train the supply provider (Overlord) if the
                            // provider is not a structure
                            supplyBuilder->train(supplyProviderType);
                        }
                    }  // closure: supplyBuilder is valid
                }      // closure: insufficient supply
            }          // closure: failed to train idle unit
        }

    }  // closure: unit iterator
}

void ExampleAIModule::onSendText(std::string text) {
    // Send the text to the game if it is not being processed.
    Broodwar->sendText("%s", text.c_str());

    // Make sure to use %s and pass the text as a parameter,
    // otherwise you may run into problems when you use the %(percent)
    // character!
}

void ExampleAIModule::onReceiveText(BWAPI::Player player, std::string text) {
    // Parse the received text
    Broodwar << player->getName() << " said \"" << text << "\"" << std::endl;
}

void ExampleAIModule::onPlayerLeft(BWAPI::Player player) {
    // Interact verbally with the other players in the game by
    // announcing that the other player has left.
    Broodwar->sendText("Goodbye %s!", player->getName().c_str());
}

void ExampleAIModule::onNukeDetect(BWAPI::Position target) {
    // Check if the target is a valid position
    if (target) {
        // if so, print the location of the nuclear strike target
        Broodwar << "Nuclear Launch Detected at " << target << std::endl;
    } else {
        // Otherwise, ask other players where the nuke is!
        Broodwar->sendText("Where's the nuke?");
    }

    // You can also retrieve all the nuclear missile targets using
    // Broodwar->getNukeDots()!
}

void ExampleAIModule::onUnitDiscover(BWAPI::Unit unit) {}

void ExampleAIModule::onUnitEvade(BWAPI::Unit unit) {}

void ExampleAIModule::onUnitShow(BWAPI::Unit unit) {}

void ExampleAIModule::onUnitHide(BWAPI::Unit unit) {}

void ExampleAIModule::onUnitCreate(BWAPI::Unit unit) {
    if (Broodwar->isReplay()) {
        // if we are in a replay, then we will print out the build order of the
        // structures
        if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral()) {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText("%.2d:%.2d: %s creates a %s", minutes, seconds, unit->getPlayer()->getName().c_str(),
                               unit->getType().c_str());
        }
    }
}

void ExampleAIModule::onUnitDestroy(BWAPI::Unit unit) {}

void ExampleAIModule::onUnitMorph(BWAPI::Unit unit) {
    if (Broodwar->isReplay()) {
        // if we are in a replay, then we will print out the build order of the
        // structures
        if (unit->getType().isBuilding() && !unit->getPlayer()->isNeutral()) {
            int seconds = Broodwar->getFrameCount() / 24;
            int minutes = seconds / 60;
            seconds %= 60;
            Broodwar->sendText("%.2d:%.2d: %s morphs a %s", minutes, seconds, unit->getPlayer()->getName().c_str(),
                               unit->getType().c_str());
        }
    }
}

void ExampleAIModule::onUnitRenegade(BWAPI::Unit unit) {}

void ExampleAIModule::onSaveGame(std::string gameName) {
    Broodwar << "The game was saved to \"" << gameName << "\"" << std::endl;
}

void ExampleAIModule::onUnitComplete(BWAPI::Unit unit) {}
