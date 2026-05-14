#include "Voxel/Generation/Noises.h"

namespace MCWorldGen::Noises
{
	const FNoiseDef Temperature       { TEXT("minecraft:temperature"),        FOctavedNoiseParameters(-10, {1.5, 0.0, 1.0, 0.0, 0.0, 0.0}) };
	const FNoiseDef Vegetation        { TEXT("minecraft:vegetation"),         FOctavedNoiseParameters( -8, {1.0, 1.0, 0.0, 0.0, 0.0, 0.0}) };
	const FNoiseDef Continentalness   { TEXT("minecraft:continentalness"),    FOctavedNoiseParameters( -9, {1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0}) };
	const FNoiseDef Erosion           { TEXT("minecraft:erosion"),            FOctavedNoiseParameters( -9, {1.0, 1.0, 0.0, 1.0, 1.0}) };
	const FNoiseDef Ridge             { TEXT("minecraft:ridge"),              FOctavedNoiseParameters( -7, {1.0, 2.0, 1.0, 0.0, 0.0, 0.0}) };

	const FNoiseDef Shift             { TEXT("minecraft:offset"),             FOctavedNoiseParameters( -3, {1.0, 1.0, 1.0, 0.0}) };

	const FNoiseDef Jagged            { TEXT("minecraft:jagged"),             FOctavedNoiseParameters(-16, {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}) };

	const FNoiseDef CaveCheese        { TEXT("minecraft:cave_cheese"),        FOctavedNoiseParameters( -8, {0.5, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 2.0, 0.0}) };

	const FNoiseDef Surface           { TEXT("minecraft:surface"),            FOctavedNoiseParameters( -6, {1.0, 1.0, 1.0}) };
	const FNoiseDef SurfaceSecondary  { TEXT("minecraft:surface_secondary"),  FOctavedNoiseParameters( -6, {1.0, 1.0, 0.0, 1.0}) };
}
