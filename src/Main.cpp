
#include "SkillExperienceBuffer.h"
#include "version.h"
#include <stddef.h>
#include <cmath>

using namespace RE::BSScript;
using namespace SKSE;
using namespace SKSE::log;
using namespace SKSE::stl;

namespace {
    const std::string PapyrusClassName = "SleepToGainExperience";
    const SKSE::SerializationInterface* g_serialization;
    struct Settings {
        std::uint32_t enableSleepTimeRequirement : 1;
        std::uint32_t : 31;
        float percentExpRequiresSleep;
        float minDaysSleepNeeded;
        float interuptedPenaltyPercent;

        Settings() { ZeroMemory(this, sizeof(Settings)); }
    };
    constexpr std::uint32_t serializationDataVersion = 1;
    RE::TESObjectWEAP* myWeap;
    void Serialization_Revert(SKSE::SerializationInterface*) { ZeroMemory(myWeap, sizeof(RE::TESObjectWEAP));
    }
    void Serialization_Save(SKSE::SerializationInterface* serializationInterface) {
        logger::info("Serialization_Save begin");

        if (serializationInterface->OpenRecord('DATA', serializationDataVersion)) {
            serializationInterface->WriteRecordData(myWeap, sizeof(RE::TESObjectWEAP));
        }

        logger::info("Serialization_Save end");
    }
    void Serialization_Load(SKSE::SerializationInterface* serializationInterface) {
        logger::info("Serialization_Load begin");
        
        std::uint32_t type;
        std::uint32_t version;
        std::uint32_t length;

        bool error = false;

        while (!error && serializationInterface->GetNextRecordInfo(type, version, length)) {
            if (type == 'DATA') {
                if (version == serializationDataVersion) {
                    if (length == sizeof(RE::TESObjectWEAP)) {
                        logger::info("read data");
                        
                        serializationInterface->ReadRecordData(myWeap, length);
                        //*myWeap = *tempWeap;
                    }

                    else {
                        logger::info("empty or invalid data");
                    }
                } else {
                    error = true;
                    logger::info(FMT_STRING("version mismatch! read data version is {}, expected {}"), version,
                                 serializationDataVersion);
                }
            } else {
                logger::info(FMT_STRING("unhandled type {:x}"), type);
                error = true;
            }
        }
        
        logger::info("Serialization_Load end");
    }
    Settings g_settings;
    SkillExperienceBuffer g_experienceBuffer;
    /**
     * Setup logging.
     *
     * <p>
     * Logging is important to track issues. CommonLibSSE bundles functionality for spdlog, a common C++ logging
     * framework. Here we initialize it, using values from the configuration file. This includes support for a debug
     * logger that shows output in your IDE when it has a debugger attached to Skyrim, as well as a file logger which
     * writes data to the standard SKSE logging directory at <code>Documents/My Games/Skyrim Special Edition/SKSE</code>
     * (or <code>Skyrim VR</code> if you are using VR).
     * </p>
     */
    void InitializeLogging() {
        auto path = log_directory();
        if (!path) {
            report_and_fail("Unable to lookup SKSE logs directory.");
        }
        *path /= PluginDeclaration::GetSingleton()->GetName();
        *path += L".log";

        std::shared_ptr<spdlog::logger> log;
        if (IsDebuggerPresent()) {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::msvc_sink_mt>());
        } else {
            log = std::make_shared<spdlog::logger>(
                "Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
        }
        log->set_level(spdlog::level::level_enum::debug);
        log->flush_on(spdlog::level::level_enum::trace);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [%t] [%s:%#] %v");
    }
    

    

    void PlayerCharacter_AdvanceSkill_Hooked(RE::PlayerCharacter* _this, RE::ActorValue skillId, float points,
                                             std::uint32_t unk1, std::uint32_t unk2) {
        static const std::string ConsoleMenu = "Console";
        static const std::string ConsoleNativeUIMenu = "Console Native UI Menu";

        auto menuManager = RE::UI::GetSingleton();

        if (skillId >= FirstSkillId && skillId <= LastSkillId && _this == RE::PlayerCharacter::GetSingleton()) //&&
            //menuManager && !menuManager->IsMenuOpen(ConsoleMenu) && !menuManager->IsMenuOpen(ConsoleNativeUIMenu))
            
           
        {
            
            //g_experienceBuffer.addExperience(skillId, g_settings.percentExpRequiresSleep * points);

            // call the original function but with points = 0
            // not sure what effect not calling it might have

            PlayerCharacter_AdvanceSkill(_this, skillId, (1.0f - g_settings.percentExpRequiresSleep) * points, unk1,
                                         unk2);
        } else
            PlayerCharacter_AdvanceSkill(_this, skillId, points, unk1, unk2);
        std::uint32_t logID = static_cast<std::uint32_t>(skillId);
        logger::debug("AdvanceSkill_Hooked end {0} {1} {2}", logID, unk1, unk2);
    }

    InterfaceExchangeMessage _nioInterface = InterfaceExchangeMessage{};
    IBodyMorphInterface* g_bodyMorphInterface;
    void setBodyMorph(RE::StaticFunctionTag* func, RE::TESObjectREFR* ref, RE::BSFixedString morphName,
        RE::BSFixedString keyName, float avalue) {
        g_bodyMorphInterface->SetMorph(ref, morphName.c_str(), keyName.c_str(), avalue);
        
    }
    bool RegisterFuncs(RE::BSScript::IVirtualMachine* registry) {
        registry->RegisterFunction("SetBodyMorph", "NiOverride", setBodyMorph, true);

        return true;
    }

    void InitializePapyrus() {
        log::trace("Initializing Papyrus binding...");
        if (GetPapyrusInterface()->Register(RegisterFuncs)) {
            log::debug("Papyrus functions bound.");
        } else {
            stl::report_and_fail("Failure to register Papyrus bindings.");
        }
    }
    bool newGame = false;
    inline constexpr REL::ID Vtbl(static_cast<std::uint64_t>(208040));
    void InitializeMessaging() {
        if (!GetMessagingInterface()->RegisterListener([](MessagingInterface::Message* message) {
            switch (message->type) {
                // Skyrim lifecycle events.
                case MessagingInterface::kPostLoad: // Called after all plugins have finished running SKSEPlugin_Load.
                    // It is now safe to do multithreaded operations, or operations against other plugins.
                case MessagingInterface::kPostPostLoad: {  // Called after all kPostLoad message handlers have run.
                    GetMessagingInterface()->Dispatch(InterfaceExchangeMessage::kMessage_ExchangeInterface,
                                                      &_nioInterface, sizeof(_nioInterface), nullptr);
                    
                    break;
                }
                case MessagingInterface::kInputLoaded: // Called when all game data has been found.
                    break;
                case MessagingInterface::kDataLoaded: {  // All ESM/ESL/ESP plugins have loaded, main menu is now
                                                         // active.
                    // It is now safe to access form data.
                    // All forms are loaded
             
                    //Serialization_Load2();

                    REL::Relocation<std::uintptr_t> PCvtbl(Vtbl);

                    PlayerCharacter_AdvanceSkill = PCvtbl.write_vfunc(0xF7, PlayerCharacter_AdvanceSkill_Hooked);
                  
                    break;
                }
                // Skyrim game events.
                case MessagingInterface::kNewGame: {  // Player starts a new game from main menu.
                    newGame = true;
                    
                    break;
                }
                case MessagingInterface::kPreLoadGame: // Player selected a game to load, but it hasn't loaded yet.
                    break;
                    // Data will be the name of the loaded save.
                case MessagingInterface::kPostLoadGame: {  // Player's selected save game has finished loading.
                    if (_nioInterface.interfaceMap) {
                        logger::info("retrieved interface");
                        g_bodyMorphInterface = dynamic_cast<IBodyMorphInterface*>(_nioInterface.interfaceMap->QueryInterface("BodyMorph"));
                        InitializePapyrus();
                    };
                    break;
                }
                    // Data will be a boolean indicating whether the load was successful.
                case MessagingInterface::kSaveGame: {  // The player has saved a game.
                    // Data will be the save name.
                    
                    break;
                }
                case MessagingInterface::kDeleteGame: // The player deleted a saved game from within the load menu.
                    break;
            }
        })) {
            stl::report_and_fail("Unable to register message listener.");
        }
    }
}  // namespace

/**
 * This if the main callback for initializing your SKSE plugin, called just before Skyrim runs its main function.
 *
 * <p>
 * This is your main entry point to your plugin, where you should initialize everything you need. Many things can't be
 * done yet here, since Skyrim has not initialized and the Windows loader lock is not released (so don't do any
 * multithreading). But you can register to listen for messages for later stages of Skyrim startup to perform such
 * tasks.
 * </p>
 */
SKSEPluginLoad(const LoadInterface* skse) {
    InitializeLogging();

    auto* plugin = PluginDeclaration::GetSingleton();
    auto version = plugin->GetVersion();
    log::info("{} {} is loading...", plugin->GetName(), version);


    Init(skse);

    char stringBuffer[16];
    // configuration
    constexpr const char* configFileName = "Data\\SKSE\\Plugins\\SleepToGainExperience.ini";
    constexpr const char* sectionName = "General";

    //auto skyVM = RE::SkyrimVM::GetSingleton();
    //auto classVM = skyVM->impl->GetObjectHandlePolicy();
    
    //classVM->GetHandleForObject();

    // general
    g_settings.enableSleepTimeRequirement = GetPrivateProfileIntA(sectionName, "bEnableSleepTimeRequirement", 0, configFileName);

    g_settings.minDaysSleepNeeded = fabsf((float)GetPrivateProfileIntA(sectionName, "iMinHoursSleepNeeded", 7, configFileName)) / 24.0f;

    GetPrivateProfileStringA(sectionName, "fPercentRequiresSleep", "1.0", stringBuffer, sizeof(stringBuffer),configFileName);

    g_settings.percentExpRequiresSleep = fminf(1.0f, fabsf(strtof(stringBuffer, nullptr)));

    GetPrivateProfileStringA(sectionName, "fInteruptedPenaltyPercent", "1.0", stringBuffer, sizeof(stringBuffer), configFileName);

    g_settings.interuptedPenaltyPercent = fminf(1.0f, fabsf(strtof(stringBuffer, nullptr)));
    InitializeMessaging();
    g_serialization = SKSE::GetSerializationInterface();

    if (!g_serialization) {
        logger::error("Serialization Interface Not Found!");
        return false;
    }
    g_serialization->SetUniqueID('TkYT');

    //g_serialization->SetRevertCallback(Serialization_Revert);
    //g_serialization->SetSaveCallback(Serialization_Save);
    //g_serialization->SetLoadCallback(Serialization_Load);
    log::info("{} has finished loading.", plugin->GetName());
    return true;
}
