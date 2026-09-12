#pragma once
#include "CoreMinimal.h"
// Generated from Tools/build_family_catalog.json by Tools/generate_build_catalog.py.
enum class EBuildPattern : uint8 { Legacy, Travel, Orbit, Rupture, Zone, Flurry, Lance, Ring, Mine, Ricochet, Smoke, Wire, Ambush, Trail, Needle, Swarm };
enum class EBuildReaction : uint8 { Chain, Echo, Bloom, Pull, Recharge, Focus };
struct FBuildFamilySpec
{
 const TCHAR* Id; const TCHAR* Owner; const TCHAR* Title; EBuildPattern Pattern;
 float Damage, Radius, Cooldown; int32 Count; float Interval;
 const TCHAR* Branches[3]; const TCHAR* Synergy;
 EBuildReaction Reaction; float ReactionFactor, ReactionRadius; int32 ReactionCount;
};
inline const FBuildFamilySpec BuildFamilies[] = {
 { TEXT("SteelTempest"), TEXT("Samurai"), TEXT("Steel Tempest"), EBuildPattern::Legacy, 24.0f, 260.0f, 4.0f, 1, 0.3f, {TEXT("RazorHalo"),TEXT("StormEye"),TEXT("AdvancingStorm")}, TEXT("StormConductor"), EBuildReaction::Chain, 0.6f, 300.0f, 3 },
 { TEXT("Heavenfall"), TEXT("Samurai"), TEXT("Heavenfall"), EBuildPattern::Legacy, 42.0f, 280.0f, 4.0f, 1, 0.5f, {TEXT("Starfall"),TEXT("HeavenAftershock"),TEXT("SeekingStar")}, TEXT("FallenConstellation"), EBuildReaction::Echo, 0.8f, 210.0f, 2 },
 { TEXT("NightThread"), TEXT("Ninja"), TEXT("Night Thread"), EBuildPattern::Legacy, 25.0f, 360.0f, 3.5f, 3, 0.25f, {TEXT("BlackWeb"),TEXT("VenomThread"),TEXT("CrossStitch")}, TEXT("ThreadSever"), EBuildReaction::Chain, 0.7f, 360.0f, 4 },
 { TEXT("VenomGarden"), TEXT("Ninja"), TEXT("Venom Garden"), EBuildPattern::Legacy, 4.0f, 340.0f, 9.0f, 8, 1.1f, {TEXT("WitheringGarden"),TEXT("WanderingGarden"),TEXT("BriarGarden")}, TEXT("PlagueHarvest"), EBuildReaction::Bloom, 4.0f, 300.0f, 1 },
 { TEXT("BladeWave"), TEXT("Samurai"), TEXT("Blade Wave"), EBuildPattern::Legacy, 20.0f, 160.0f, 1.0f, 1, 0.2f, {TEXT("ReturningBlade"),TEXT("CrossingBlades"),TEXT("SplinterWave")}, TEXT("VenomEdge"), EBuildReaction::Bloom, 0.6f, 240.0f, 1 },
 { TEXT("CrescentReaper"), TEXT("Samurai"), TEXT("Crescent Reaper"), EBuildPattern::Travel, 28.0f, 120.0f, 4.5f, 7, 0.12f, {TEXT("ReapingReturn"),TEXT("TwinCrescents"),TEXT("CrimsonCrescent")}, TEXT("ReapersInvitation"), EBuildReaction::Pull, 0.6f, 300.0f, 1 },
 { TEXT("IronOrbit"), TEXT("Samurai"), TEXT("Iron Orbit"), EBuildPattern::Orbit, 7.0f, 85.0f, 6.0f, 20, 0.2f, {TEXT("CounterOrbit"),TEXT("SatelliteBlades"),TEXT("OrbitShrapnel")}, TEXT("OrbitRelay"), EBuildReaction::Recharge, 0.5f, 180.0f, 1 },
 { TEXT("FaultLine"), TEXT("Samurai"), TEXT("Fault Line"), EBuildPattern::Rupture, 30.0f, 170.0f, 5.0f, 3, 0.3f, {TEXT("CrossFault"),TEXT("RollingAftershock"),TEXT("RuptureLift")}, TEXT("FaultDetonator"), EBuildReaction::Echo, 0.75f, 250.0f, 2 },
 { TEXT("WarBanner"), TEXT("Samurai"), TEXT("War Banner"), EBuildPattern::Zone, 10.0f, 300.0f, 7.0f, 4, 1.0f, {TEXT("MarchingBanner"),TEXT("RallyingBanner"),TEXT("BannerOfThorns")}, TEXT("RallyingShadows"), EBuildReaction::Recharge, 1.0f, 260.0f, 1 },
 { TEXT("ThousandCuts"), TEXT("Samurai"), TEXT("Thousand Cuts"), EBuildPattern::Flurry, 9.0f, 75.0f, 4.0f, 6, 0.15f, {TEXT("PassingSentence"),TEXT("SharedSentence"),TEXT("OpenWounds")}, TEXT("ExecutionWindow"), EBuildReaction::Focus, 2.0f, 150.0f, 1 },
 { TEXT("SpiritLance"), TEXT("Samurai"), TEXT("Spirit Lance"), EBuildPattern::Lance, 38.0f, 70.0f, 4.5f, 1, 0.2f, {TEXT("CrossedLances"),TEXT("ReboundingLance"),TEXT("SpectralBrand")}, TEXT("ShadowSpear"), EBuildReaction::Chain, 0.65f, 440.0f, 3 },
 { TEXT("BloodMoon"), TEXT("Samurai"), TEXT("Blood Moon"), EBuildPattern::Ring, 18.0f, 140.0f, 5.0f, 4, 0.25f, {TEXT("SecondMoon"),TEXT("RedTide"),TEXT("MoonRecoil")}, TEXT("EclipseHarvest"), EBuildReaction::Bloom, 1.0f, 320.0f, 1 },
 { TEXT("LotusMines"), TEXT("Ninja"), TEXT("Lotus Mines"), EBuildPattern::Mine, 30.0f, 180.0f, 6.0f, 3, 0.6f, {TEXT("DoublePetals"),TEXT("PatientLotus"),TEXT("VenomPetals")}, TEXT("LotusExecution"), EBuildReaction::Echo, 0.8f, 260.0f, 2 },
 { TEXT("ShadowShuriken"), TEXT("Ninja"), TEXT("Shadow Shuriken"), EBuildPattern::Ricochet, 16.0f, 380.0f, 4.0f, 5, 0.12f, {TEXT("ReturningStar"),TEXT("SplittingStar"),TEXT("ToxicStar")}, TEXT("SteelRicochet"), EBuildReaction::Chain, 0.9f, 420.0f, 3 },
 { TEXT("SmokeLattice"), TEXT("Ninja"), TEXT("Smoke Lattice"), EBuildPattern::Smoke, 6.0f, 290.0f, 6.5f, 5, 0.8f, {TEXT("DriftingSmoke"),TEXT("DoubleVeil"),TEXT("ChokingFinale")}, TEXT("HiddenBlade"), EBuildReaction::Focus, 3.0f, 200.0f, 1 },
 { TEXT("ThunderWire"), TEXT("Ninja"), TEXT("Thunder Wire"), EBuildPattern::Wire, 14.0f, 65.0f, 5.0f, 3, 0.45f, {TEXT("CrossWire"),TEXT("LiveWire"),TEXT("VenomCable")}, TEXT("GroundedSteel"), EBuildReaction::Pull, 1.0f, 360.0f, 1 },
 { TEXT("PhantomAmbush"), TEXT("Ninja"), TEXT("Phantom Ambush"), EBuildPattern::Ambush, 25.0f, 280.0f, 5.0f, 2, 0.35f, {TEXT("ThirdShadow"),TEXT("LingeringShadow"),TEXT("AssassinsInk")}, TEXT("PhantomHandoff"), EBuildReaction::Recharge, 0.75f, 200.0f, 1 },
 { TEXT("CaltropTrail"), TEXT("Ninja"), TEXT("Caltrop Trail"), EBuildPattern::Trail, 12.0f, 130.0f, 1.3f, 1, 0.4f, {TEXT("ForkedTrail"),TEXT("PersistentSpikes"),TEXT("BarbedExit")}, TEXT("SpikedOpening"), EBuildReaction::Bloom, 1.6f, 230.0f, 1 },
 { TEXT("CrimsonNeedle"), TEXT("Ninja"), TEXT("Crimson Needle"), EBuildPattern::Needle, 40.0f, 90.0f, 4.0f, 1, 0.2f, {TEXT("TwinNeedles"),TEXT("NeedleFan"),TEXT("SepticNeedle")}, TEXT("CrimsonVerdict"), EBuildReaction::Focus, 1.25f, 120.0f, 1 },
 { TEXT("RavenSwarm"), TEXT("Ninja"), TEXT("Raven Swarm"), EBuildPattern::Swarm, 8.0f, 100.0f, 5.5f, 6, 0.5f, {TEXT("CarrionFlight"),TEXT("MurderOfCrows"),TEXT("LastCry")}, TEXT("CarrionFeast"), EBuildReaction::Bloom, 2.0f, 280.0f, 1 },
};
static_assert(UE_ARRAY_COUNT(BuildFamilies)==20);
