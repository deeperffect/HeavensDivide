#pragma once
#include "CoreMinimal.h"
// Generated from Tools/build_family_catalog.json by Tools/generate_build_catalog.py.
enum class EBuildPattern : uint8 { Legacy, Orbit, Zone, Ring, Ambush, Needle, Swarm };
struct FBuildFamilySpec
{
 const TCHAR* Id; const TCHAR* Owner; const TCHAR* Title; EBuildPattern Pattern;
 float Damage, Radius, Cooldown; int32 Count; float Interval;
 const TCHAR* Branches[3]; bool Available;
};
inline const FBuildFamilySpec BuildFamilies[] = {
 { TEXT("SteelTempest"), TEXT("Samurai"), TEXT("Steel Tempest"), EBuildPattern::Legacy, 24.0f, 260.0f, 4.0f, 1, 0.3f, {TEXT("RazorHalo"),TEXT("StormEye"),TEXT("AdvancingStorm")}, false },
 { TEXT("Heavenfall"), TEXT("Samurai"), TEXT("Heavenfall"), EBuildPattern::Legacy, 42.0f, 280.0f, 4.0f, 1, 0.5f, {TEXT("Starfall"),TEXT("HeavenAftershock"),TEXT("SeekingStar")}, false },
 { TEXT("NightThread"), TEXT("Ninja"), TEXT("Night Thread"), EBuildPattern::Legacy, 25.0f, 360.0f, 3.5f, 3, 0.25f, {TEXT("BlackWeb"),TEXT("VenomThread"),TEXT("CrossStitch")}, false },
 { TEXT("VenomGarden"), TEXT("Ninja"), TEXT("Venom Garden"), EBuildPattern::Legacy, 4.0f, 340.0f, 9.0f, 8, 1.1f, {TEXT("WitheringGarden"),TEXT("WanderingGarden"),TEXT("BriarGarden")}, false },
 { TEXT("BladeWave"), TEXT("Samurai"), TEXT("Blade Wave"), EBuildPattern::Legacy, 20.0f, 160.0f, 1.0f, 1, 0.2f, {TEXT("ReturningBlade"),TEXT("CrossingBlades"),TEXT("SplinterWave")}, true },
 { TEXT("IronOrbit"), TEXT("Samurai"), TEXT("Iron Orbit"), EBuildPattern::Orbit, 7.0f, 85.0f, 6.0f, 20, 0.2f, {TEXT("CounterOrbit"),TEXT("SatelliteBlades"),TEXT("OrbitShrapnel")}, false },
 { TEXT("WarBanner"), TEXT("Samurai"), TEXT("War Banner"), EBuildPattern::Zone, 10.0f, 300.0f, 7.0f, 4, 1.0f, {TEXT("MarchingBanner"),TEXT("RallyingBanner"),TEXT("BannerOfThorns")}, false },
 { TEXT("BloodMoon"), TEXT("Samurai"), TEXT("Blood Moon"), EBuildPattern::Ring, 18.0f, 140.0f, 5.0f, 4, 0.25f, {TEXT("SecondMoon"),TEXT("RedTide"),TEXT("MoonRecoil")}, false },
 { TEXT("PhantomAmbush"), TEXT("Ninja"), TEXT("Phantom Ambush"), EBuildPattern::Ambush, 25.0f, 280.0f, 5.0f, 2, 0.35f, {TEXT("ThirdShadow"),TEXT("LingeringShadow"),TEXT("AssassinsInk")}, false },
 { TEXT("CrimsonNeedle"), TEXT("Ninja"), TEXT("Crimson Needle"), EBuildPattern::Needle, 40.0f, 90.0f, 4.0f, 1, 0.2f, {TEXT("TwinNeedles"),TEXT("NeedleFan"),TEXT("SepticNeedle")}, false },
 { TEXT("RavenSwarm"), TEXT("Ninja"), TEXT("Raven Swarm"), EBuildPattern::Swarm, 8.0f, 100.0f, 5.5f, 6, 0.5f, {TEXT("CarrionFlight"),TEXT("MurderOfCrows"),TEXT("LastCry")}, false },
};
inline constexpr int32 BuildFamilyCount=UE_ARRAY_COUNT(BuildFamilies);
