#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <android/log.h>
#include <jni.h>

#include "dl_internal.h"
#include "mcpelauncher.h"
#include "modscript.h"
#include "dobby_public.h"

#define DLSYM_DEBUG

typedef bool cppbool;

#include "modscript_structs.h"

typedef void Minecraft;

typedef Player LocalPlayer;

typedef struct {
	float ticksPerSecond; //0
	int elapsedTicks; //4
	float renderPartialTicks; //8
	float timerSpeed; //12
	float elapsedPartialTicks; //16
	
} MCPETimer;

typedef struct {
	void* ptr;
} unique_ptr;

#define GAMEMODE_VTABLE_OFFSET_USE_ITEM_ON 11
#define GAMEMODE_VTABLE_OFFSET_ATTACK 15
#define GAMEMODE_VTABLE_OFFSET_TICK 8
#define GAMEMODE_VTABLE_OFFSET_INIT_PLAYER 12
// from HumanoidMobRenderer::additionalRendering
#define ENTITY_VTABLE_OFFSET_GET_CARRIED_ITEM 138
// from MobRenderer::bindMobTexture
#define MOB_VTABLE_OFFSET_GET_TEXTURE 107
// from Entity::save
#define ENTITY_VTABLE_OFFSET_GET_ENTITY_TYPE_ID 77
// from Player::getSelectedItem
#ifdef __i386
// FIXME 0.11
#define PLAYER_INVENTORY_OFFSET 3256
#else
#define PLAYER_INVENTORY_OFFSET 3256
#endif
#define MINECRAFT_VTABLE_OFFSET_UPDATE 21
#define MINECRAFT_VTABLE_OFFSET_SET_LEVEL 30
// this is / 4 bytes already; found in Mob::actuallyHurt
#define MOB_HEALTH_OFFSET 91
// found in TextureAtlas::load
#define APPPLATFORM_VTABLE_OFFSET_READ_ASSET_FILE 15
// from calls to Timer::advanceTime
#define MINECRAFT_TIMER_OFFSET 216
// from Entity::setPos(Vec3 const&)
#define ENTITY_VTABLE_OFFSET_SETPOS 4
// from Minecraft::selectLevel
#define MINECRAFT_LEVEL_OFFSET 244
#ifdef __i386
// 0xf4
// FIXME 0.11
#define MINECRAFT_LOCAL_PLAYER_OFFSET 332
#else
#define MINECRAFT_LOCAL_PLAYER_OFFSET 332 // MinecraftClient::selectLevel; look for constructor
#endif
#define GAMERENDERER_GETFOV_SIZE 0xbc
// MinecartRideable::interactWithPlayer
#define ENTITY_VTABLE_OFFSET_START_RIDING 25
// LegacyClientNetworkHandler::handleEntityLink
#define ENTITY_VTABLE_OFFSET_STOP_RIDING 100
// already /4; from Inventory::selectSlot
#define INVENTORY_SELECTED_SLOT_OFFSET 8
// Touch::StartMenuScreen::handleBackEvent + 2(!)
#define MINECRAFT_VTABLE_OFFSET_QUIT 23

#define LOG_TAG "BlockLauncher/ModScript"
#define FALSE 0
#define TRUE 1

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

#define ITEMID 0
#define DAMAGE 1
#define AMOUNT 2

//This is true on the ARM/Dalvik/bionic platform

//(0x23021C - 0x2301e8) / 4

JavaVM* bl_JavaVM;

//TODO share headers
void bl_setuphooks_cppside();
void bl_changeEntitySkin(void* entity, const char* newSkin);
const char* bl_getCharArr(void* str);
void bl_attachLevelListener();
void bl_renderManager_setRenderType(Entity* entity, int type);
void bl_renderManager_clearRenderTypes();
void bl_cppNewLevelInit();
void bl_clearNameTags();
void bl_sendIdentPacket();
void* bl_marauder_translation_function(void* input);

jclass bl_scriptmanager_class;

static void (*bl_GameMode_useItemOn_real)(void*, Player*, ItemInstance*, TilePos*, signed char, Vec3*);
static void (*bl_Minecraft_setLevel_real)(Minecraft*, unique_ptr*, cppstr*, LocalPlayer*);
void* (*bl_Minecraft_selectLevel_real)(void*, Minecraft*, void*, void*, void*);
static void (*bl_Minecraft_leaveGame_real)(Minecraft*, int);
static void (*bl_TileSource_setTileAndData) (TileSource*, int, int, int, FullTile*, int);
static void (*bl_GameMode_attack_real)(void*, Player*, Entity*);
static ItemInstance* (*bl_Player_getCarriedItem)(Player*);
static void (*bl_Entity_setPos)(Entity*, float, float, float);
static void (*bl_Level_explode)(Level*, Entity*, float, float, float, float, int);
static int (*bl_Inventory_add)(void*, ItemInstance*);
static void (*bl_Level_addEntity)(Level*, Entity*);
int (*bl_TileSource_getData)(TileSource*, int, int, int);
static void (*bl_Level_setNightMode)(Level*, int);
static void (*bl_Entity_setRot)(Entity*, float, float);
static void (*bl_GameMode_tick_real)(void*);
static int (*bl_TileSource_getRawBrightness)(unsigned char*, TileSource*, int, int, int, cppbool);
static void (*bl_GameMode_initPlayer_real)(void*, Player*);
static float (*bl_GameRenderer_getFov)(void*, float, int);
static float (*bl_GameRenderer_getFov_real)(void*, float, int);
static void (*bl_NinecraftApp_onGraphicsReset)(Minecraft*);
static void* (*bl_Mob_getTexture)(Entity*);
static void (*bl_LocalPlayer_hurtTo)(Player*, int);
static void (*bl_Entity_remove)(Entity*);
static void (*bl_AgebleMob_setAge)(Entity*, int);
static void (*bl_GameMode_destroyBlock_real)(void*, Player*, int, int, int, signed char);
static Entity* (*bl_Entity_spawnAtLocation)(void*, ItemInstance*, float);
static long (*bl_Level_getTime)(Level*);
static void (*bl_Level_setTime)(Level*, long); //yes, that's a 32-bit long. I have no clue why this was float before.
static void* (*bl_Level_getLevelData)(Level*);
static void (*bl_LevelData_setSpawn)(void*, TilePos*);
static void (*bl_LevelData_setGameType)(void*, int);
static int (*bl_LevelData_getGameType)(void*);
static void (*bl_Entity_setOnFire)(Entity*, int);
void* (*bl_TileSource_getTileEntity)(TileSource*, int, int, int);
static void (*bl_ChestTileEntity_setItem)(void*, int, ItemInstance*);
static ItemInstance* (*bl_ChestTileEntity_getItem)(void*, int);
static void (*bl_FurnaceTileEntity_setItem)(void*, int, ItemInstance*);
static ItemInstance* (*bl_FurnaceTileEntity_getItem)(void*, int);
static int (*bl_FillingContainer_clearSlot)(void*, int);
static ItemInstance* (*bl_FillingContainer_getItem)(void*, int);
static void (*bl_Minecraft_setIsCreativeMode)(Minecraft*, int);
ItemInstance* (*bl_Player_getArmor)(Player*, int);
static void  (*bl_Player_setArmor)(Player*, int, ItemInstance*);
static void (*bl_Inventory_clearInventoryWithDefault)(void*);
static void (*bl_Inventory_Inventory)(void*, Player*, cppbool);
static void (*bl_Inventory_delete1_Inventory)(void*);
void (*bl_ItemInstance_setId)(ItemInstance*, int);
int (*bl_ItemInstance_getId)(ItemInstance*);
static void (*bl_NinecraftApp_update_real)(Minecraft*);
static void (*bl_FillingContainer_replaceSlot)(void*, int, ItemInstance*);
static void (*bl_HumanoidModel_constructor_real)(HumanoidModel*, float, float);
static void (*bl_EnderManModel_constructor_real)(HumanoidModel*);
void (*bl_ModelPart_addBox)(ModelPart*, float, float, float, int, int, int, float);

static void (*bl_SurvivalMode_startDestroyBlock_real)(void*, Player*, int, int, int, signed char);
static void (*bl_CreativeMode_startDestroyBlock_real)(void*, Player*, int, int, int, signed char);

static void (*bl_LevelRenderer_allChanged)(void*);

static int (*bl_FillingContainer_removeResource)(void*, ItemInstance*, bool);

static soinfo2* mcpelibhandle = NULL;

Level* bl_level;
Minecraft* bl_minecraft;
void* bl_gamemode;
Player* bl_localplayer;
static int bl_hasinit_script = 0;
bool preventDefaultStatus = false;
static float bl_newfov = -1.0f;

Entity* bl_removedEntity = NULL;

int bl_frameCallbackRequested = 0;

static int bl_hasinit_prepatch = 0;

static unsigned char getFovOriginal[GAMERENDERER_GETFOV_SIZE];
static unsigned char getFovHooked[GAMERENDERER_GETFOV_SIZE];

extern bool bl_onLockDown;

static bool bl_untampered;

#ifdef DLSYM_DEBUG

void* debug_dlsym(void* handle, const char* symbol) {
	dlerror();
	void* retval = dlsym(handle, symbol);
	const char* err = dlerror();
	if (err) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Error in ModPE script init: %s\n", err);
	}
	return retval;
}

#define dlsym debug_dlsym
#endif //DLSYM_DEBUG

Entity* bl_getEntityWrapper(Level* level, long long entityId);

void bl_setItemInstance(ItemInstance* instance, int id, int count, int damage) {
	instance->damage = damage;
	instance->count = count;
	bl_ItemInstance_setId(instance, id);
}

ItemInstance* bl_newItemInstance(int id, int count, int damage) {
	ItemInstance* instance = (ItemInstance*) malloc(sizeof(ItemInstance));
	bl_setItemInstance(instance, id, count, damage);
	return instance;
}

void bl_Entity_setPos_helper(Entity* entity, float x, float y, float z) {
	void (*setPos)(Entity*, float, float, float);
	setPos = entity->vtable[ENTITY_VTABLE_OFFSET_SETPOS];
	setPos(entity, x, y, z);
}

extern unsigned char bl_TileSource_getTile(TileSource* source, int x, int y, int z);

#ifndef __arm__
static void bl_panicTamper() {
	((int*) 0x0) = 0x0;
}
#else
extern void bl_panicTamper();
#endif

void bl_GameMode_useItemOn_hook(void* gamemode, Player* player, ItemInstance* itemStack,
	TilePos* pos, signed char side, Vec3* vec3) {
	JNIEnv *env;
	int x = pos->x;
	int y = pos->y;
	int z = pos->z;
	//bl_level = level;
	Level* level = bl_level;
	bl_localplayer = player;

	if (!bl_untampered) {
		bl_panicTamper();
		return;
	}

	preventDefaultStatus = FALSE;
	int itemId = 0;
	int itemDamage = 0;
	if (itemStack != NULL) {
		itemId = bl_ItemInstance_getId(itemStack);
		itemDamage = itemStack->damage;
	}

	int blockId = bl_TileSource_getTile(level->tileSource, x, y, z);
	int blockDamage = bl_TileSource_getData(level->tileSource, x, y, z);

#ifdef EXTREME_LOGGING
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "use item on: JavaVM = %p\n", bl_JavaVM);
#endif
	(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);

#ifdef EXTREME_LOGGING
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "use item on: env = %p\n", env);
#endif

	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "useItemOnCallback", "(IIIIIIII)V");

#ifdef EXTREME_LOGGING
	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "use item on: mid = %i, class = %p\n", mid, bl_scriptmanager_class);
#endif

	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, x, y, z, itemId, blockId, side, itemDamage, blockDamage);

	(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);

	if (!preventDefaultStatus) bl_GameMode_useItemOn_real(gamemode, player, itemStack, pos, side, vec3);
}

void bl_SurvivalMode_startDestroyBlock_hook(void* gamemode, Player* player, int x, int y, int z, signed char side) {
	JNIEnv *env;
	preventDefaultStatus = FALSE;

	int attachStatus = (*bl_JavaVM)->GetEnv(bl_JavaVM, (void**) &env, JNI_VERSION_1_2);
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);
	}

	
	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "startDestroyBlockCallback", "(IIII)V");
	
	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, x, y, z, side);
	
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);
	}
	
	if(!preventDefaultStatus) bl_SurvivalMode_startDestroyBlock_real(gamemode, player, x, y, z, side);
}

void bl_CreativeMode_startDestroyBlock_hook(void* gamemode, Player* player, int x, int y, int z, signed char side) {
	JNIEnv *env;
	preventDefaultStatus = FALSE;

	int attachStatus = (*bl_JavaVM)->GetEnv(bl_JavaVM, (void**) &env, JNI_VERSION_1_2);
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);
	}

	
	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "startDestroyBlockCallback", "(IIII)V");
	
	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, x, y, z, side);
	
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);
	}
	if(!preventDefaultStatus) bl_CreativeMode_startDestroyBlock_real(gamemode, player, x, y, z, side);
}

void bl_Minecraft_setLevel_hook(Minecraft* minecraft, unique_ptr* levelPtr, cppstr* levelName, LocalPlayer* player) {
	JNIEnv *env;

	Level* level = levelPtr->ptr;

	bl_localplayer = player;
	bl_minecraft = minecraft;
	bl_level = level;

	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Set level called");

	//This hook can be triggered by ModPE scripts, so don't attach/detach when already executing in Java thread
	int attachStatus = (*bl_JavaVM)->GetEnv(bl_JavaVM, (void**) &env, JNI_VERSION_1_2);
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);
	}

	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "setLevelCallback", "(ZZ)V");

	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, (int) (level != NULL), (jboolean) level->isRemote);

	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);
	}

	bl_Minecraft_setLevel_real(minecraft, levelPtr, levelName, player);

	//attach the listener
	/*bl_attachLevelListener();*/
	bl_clearNameTags();
	/*bl_renderManager_clearRenderTypes();
	bl_cppNewLevelInit();
	if (level->isRemote) {
		bl_sendIdentPacket();
	}*/
}

void* bl_Minecraft_selectLevel_hook(void* retval2, Minecraft* minecraft, void* wDir, void* wName, void* levelSettings) {
	if (!bl_untampered) {
		bl_panicTamper();
		return NULL;
	}
	bl_minecraft = minecraft;
	JNIEnv *env;

	//This hook can be triggered by ModPE scripts, so don't attach/detach when already executing in Java thread
	int attachStatus = (*bl_JavaVM)->GetEnv(bl_JavaVM, (void**) &env, JNI_VERSION_1_2);
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);
	}

	// Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "selectLevelCallback", "(Ljava/lang/String;Ljava/lang/String;)V");

	
	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, (*env)->NewStringUTF(env, bl_getCharArr(wName)),
		(*env)->NewStringUTF(env, bl_getCharArr(wDir)));

	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);
	}

	void* retval = bl_Minecraft_selectLevel_real(retval2, minecraft, wDir, wName, levelSettings);
	bl_level = *((Level**) ((uintptr_t) minecraft + MINECRAFT_LEVEL_OFFSET));
	bl_localplayer = *((Entity**) ((uintptr_t) minecraft + MINECRAFT_LOCAL_PLAYER_OFFSET));
	bl_onLockDown = false;
	return retval;
}

void bl_Minecraft_leaveGame_hook(Minecraft* minecraft, int thatotherboolean) {
	JNIEnv *env;
	bl_Minecraft_leaveGame_real(minecraft, thatotherboolean);

	//This hook can be triggered by ModPE scripts, so don't attach/detach when already executing in Java thread
	int attachStatus = (*bl_JavaVM)->GetEnv(bl_JavaVM, (void**) &env, JNI_VERSION_1_2);
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);
	}

	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "leaveGameCallback", "(Z)V");

	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, thatotherboolean);

	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);
	}

}

void bl_GameMode_attack_hook(void* gamemode, Player* player, Entity* entity) {
	JNIEnv *env;
	bl_localplayer = player;
	preventDefaultStatus = FALSE;
	(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);

	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "attackCallback", "(JJ)V");

	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, player->entityId, entity->entityId);

	(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);

	if (!preventDefaultStatus) bl_GameMode_attack_real(gamemode, player, entity);
}

void bl_GameMode_tick_hook(void* gamemode) {
	JNIEnv *env;

	bl_level = *((Level**) ((uintptr_t) bl_minecraft + MINECRAFT_LEVEL_OFFSET));

	bl_localplayer = *((Entity**) ((uintptr_t) bl_minecraft + MINECRAFT_LOCAL_PLAYER_OFFSET));

	(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);

	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "tickCallback", "()V");

	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid);

	(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);
	bl_GameMode_tick_real(gamemode);
}

void bl_GameMode_initPlayer_hook(void* gamemode, Player* player) {
	bl_GameMode_initPlayer_real(gamemode, player);
	bl_localplayer = player;
	bl_gamemode = gamemode;
}

void bl_GameMode_destroyBlock_hook(void* gamemode, Player* player, int x, int y, int z, signed char side){
	JNIEnv *env;
	preventDefaultStatus = FALSE;
	(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);

	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "destroyBlockCallback", "(IIII)V");

	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid, x, y, z, side);

	(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);

	if (!preventDefaultStatus) bl_GameMode_destroyBlock_real(gamemode, player, x, y, z, side);
}

void bl_handleFrameCallback() {
	JNIEnv *env;
	//This hook can be triggered by ModPE scripts, so don't attach/detach when already executing in Java thread
	int attachStatus = (*bl_JavaVM)->GetEnv(bl_JavaVM, (void**) &env, JNI_VERSION_1_2);
	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->AttachCurrentThread(bl_JavaVM, &env, NULL);
	}

	//Call back across JNI into the ScriptManager
	jmethodID mid = (*env)->GetStaticMethodID(env, bl_scriptmanager_class, "frameCallback", "()V");

	(*env)->CallStaticVoidMethod(env, bl_scriptmanager_class, mid);

	if (attachStatus == JNI_EDETACHED) {
		(*bl_JavaVM)->DetachCurrentThread(bl_JavaVM);
	}
}

void bl_NinecraftApp_update_hook(Minecraft* minecraft) {
	bl_NinecraftApp_update_real(minecraft);
	if (bl_frameCallbackRequested) {
		bl_frameCallbackRequested = 0;
		bl_handleFrameCallback();
	}
}
extern void bl_cape_hook(HumanoidModel* self, float scale, float y);

static bool bl_inEnderManModelConstructor = false;

void bl_HumanoidModel_constructor_hook(HumanoidModel* self, float scale, float y) {
	bl_HumanoidModel_constructor_real(self, scale, y);
	bl_cape_hook(self, scale, y);
	if (bl_inEnderManModelConstructor) return;
	int oldTextureOffsetX = self->bipedHead.textureOffsetX;
	self->bipedHead.textureOffsetX = 32;
	bl_ModelPart_addBox(&self->bipedHead, -4.0F, -8.0F, -4.0F, 8, 8, 8, scale + 0.5F);
	self->bipedHead.textureOffsetX = oldTextureOffsetX;
	self->bipedHead.material = &self->materialAlphaTest;
}

void bl_EnderManModel_constructor_hook(HumanoidModel* self) {
	bl_inEnderManModelConstructor = true;
	bl_EnderManModel_constructor_real(self);
	bl_inEnderManModelConstructor = false;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeAddItemChest
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot, jint id, jint damage, jint amount) {
	if (bl_level == NULL) return;
	ItemInstance* instance = bl_newItemInstance(id, amount, damage);

	void* te = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	if (te == NULL) return;
	bl_ChestTileEntity_setItem(te, slot, instance);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetItemChest
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot) {
	if (bl_level == NULL) return -1;

	void* te = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	if (te == NULL) return -1;
	ItemInstance* instance = bl_ChestTileEntity_getItem(te, slot);
	if (instance == NULL) return 0;
	return bl_ItemInstance_getId(instance);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetItemDataChest
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot) {
	if (bl_level == NULL) return -1;

	void* te = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	if (te == NULL) return -1;
	ItemInstance* instance = bl_ChestTileEntity_getItem(te, slot);
	if (instance == NULL) return 0;
	return instance->damage;
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetItemCountChest
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot) {
	if (bl_level == NULL) return -1;

	void* te = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	if (te == NULL) return -1;
	ItemInstance* instance = bl_ChestTileEntity_getItem(te, slot);
	if (instance == NULL) return 0;
	return instance->count;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetTime
  (JNIEnv *env, jclass clazz, jlong time) {
	if (bl_level == NULL) return;
	bl_Level_setTime(bl_level, time);
	//void* levelRenderer = *((void**) (((uintptr_t) bl_minecraft) + 212));
	//bl_LevelRenderer_allChanged(levelRenderer);
}

JNIEXPORT jlong JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetTime
  (JNIEnv *env, jclass clazz) {
	if (bl_level == NULL) return 0;
	return bl_Level_getTime(bl_level);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetSpawn
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {
	if (bl_level == NULL) return;
	void* levelData = bl_Level_getLevelData(bl_level);
	TilePos tilePos;
	tilePos.x = x;
	tilePos.y = y;
	tilePos.z = z;
	bl_LevelData_setSpawn(levelData, &tilePos);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetGameType
  (JNIEnv *env, jclass clazz, jint type) {
	if (bl_level == NULL) return;
	void* levelData = bl_Level_getLevelData(bl_level);
	bl_LevelData_setGameType(levelData, type);
	if (bl_localplayer == NULL) return;
	bl_Minecraft_setIsCreativeMode(bl_minecraft, type == 1);
	void* invPtr = *((void**) (((intptr_t) bl_localplayer) + PLAYER_INVENTORY_OFFSET)); //TODO fix this for 0.7.2
	//((char*) invPtr)[32] = type == 1;
	//bl_Inventory_clearInventoryWithDefault(invPtr);
	bl_Inventory_delete1_Inventory(invPtr);
	bl_Inventory_Inventory(invPtr, bl_localplayer, type == 1);
	//int dim = type == 1? 10: 0; //daylight cycle
	//bl_LevelData_setDimension(levelData, dim);
}


JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetGameType
  (JNIEnv *env, jclass clazz) {
	if (bl_level == NULL) return 0;
	void* levelData = bl_Level_getLevelData(bl_level);
	return bl_LevelData_getGameType(levelData);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeDestroyBlock
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {

	if(bl_gamemode == NULL) return;

	bl_GameMode_destroyBlock_real(bl_gamemode, bl_localplayer, x, y, z, 2);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetOnFire
  (JNIEnv *env, jclass clazz, jlong entityId, jint howLong) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	bl_Entity_setOnFire(entity, howLong);
}

float bl_GameRenderer_getFov_hook(void* gameRenderer, float datFloat, int datBoolean) {
	return bl_newfov;
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetCarriedItem
  (JNIEnv *env, jclass clazz, jint type) {
	if (bl_localplayer == NULL) return 0;
	ItemInstance* instance = bl_Player_getCarriedItem(bl_localplayer);
	if (instance == NULL) return 0;

	switch (type) {
		case ITEMID:
			return bl_ItemInstance_getId(instance);
		case DAMAGE:
			return instance->damage;
		case AMOUNT:
			return instance->count;
	}
	return 0;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetTile
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint id, jint damage) {
	FullTile tile;
	tile.id = id;
	tile.data = damage;
	bl_TileSource_setTileAndData(bl_level->tileSource, x, y, z, &tile, 3); //3 = full block update
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeHurtTo
  (JNIEnv *env, jclass clazz, jint to) {
	bl_LocalPlayer_hurtTo(bl_localplayer, to); //3 = full block update
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativePreventDefault
  (JNIEnv *env, jclass clazz) {
	preventDefaultStatus = TRUE;
}

JNIEXPORT jlong JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetPlayerEnt
  (JNIEnv *env, jclass clazz) {
	if (bl_localplayer == NULL) return 0;
	return bl_localplayer->entityId;
}

JNIEXPORT jlong JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetLevel
  (JNIEnv *env, jclass clazz) {
	return (jlong) (intptr_t) bl_level;
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetBrightness
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {
	unsigned char retval;
	bl_TileSource_getRawBrightness(&retval, bl_level->tileSource, x, y, z, 1); //all observed uses of getRawBrightness pass true
	return retval;
}

JNIEXPORT jfloat JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetPlayerLoc
  (JNIEnv *env, jclass clazz, jint axis) {
	if (bl_localplayer == NULL) return 0;
	switch (axis) {
		case AXIS_X:
			return bl_localplayer->x;
		case AXIS_Y:
			return bl_localplayer->y;
		case AXIS_Z:
			return bl_localplayer->z;
	}
	return 0;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetPosition
  (JNIEnv *env, jclass clazz, jlong entityId, jfloat x, jfloat y, jfloat z) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	bl_Entity_setPos_helper(entity, x, y, z);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetVel
  (JNIEnv *env, jclass clazz, jlong entityId, jfloat vel, jint axis) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	//the iOS version probably uses Entity::lerpMotion; that's too mainstream so we set velocity directly
	switch (axis) {
		case AXIS_X:
			entity->motionX = vel;
			break;
		case AXIS_Y:
			entity->motionY = vel;
			break;
		case AXIS_Z:
			entity->motionZ = vel;
			break;
	}
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeRideAnimal
  (JNIEnv *env, jclass clazz, jlong riderId, jlong mountId) {
	Entity* rider = bl_getEntityWrapper(bl_level, riderId);
	Entity* mount = bl_getEntityWrapper(bl_level, mountId);
	if (rider == NULL) return;
	if (mount == NULL) {
		void* vtable = rider->vtable[ENTITY_VTABLE_OFFSET_STOP_RIDING];
		void (*fn)(Entity*, bool) = vtable;
		fn(rider, true);
	} else {
		void* vtable = rider->vtable[ENTITY_VTABLE_OFFSET_START_RIDING];
		void (*fn)(Entity*, Entity*) = (void (*) (Entity*, Entity*)) vtable;
		fn(rider, mount);
	}
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeExplode
  (JNIEnv *env, jclass clazz, jfloat x, jfloat y, jfloat z, jfloat power) {
	bl_Level_explode(bl_level, bl_localplayer, x, y, z, power, FALSE);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeAddItemInventory
  (JNIEnv *env, jclass clazz, jint id, jint amount, jint damage) {
	if (bl_localplayer == NULL) return;
	bool remove = amount < 0;
	if (remove) amount *= -1;
	ItemInstance* instance = bl_newItemInstance(id, amount, damage);
	//we grab the inventory instance from the player
	void* invPtr = *((void**) (((uintptr_t) bl_localplayer) + PLAYER_INVENTORY_OFFSET)); //TODO fix this for 0.7.2
	if (!remove) {
		bl_Inventory_add(invPtr, instance);
	} else {
		bl_FillingContainer_removeResource(invPtr, instance, 0);
	}
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetNightMode
  (JNIEnv *env, jclass clazz, jboolean nightMode) {
	bl_Level_setNightMode(bl_level, nightMode);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetTile
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {
	return bl_TileSource_getTile(bl_level->tileSource, x, y, z);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetData
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {
	return bl_TileSource_getData(bl_level->tileSource, x, y, z);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetPositionRelative
  (JNIEnv *env, jclass clazz, jlong entityId, jfloat deltax, jfloat deltay, jfloat deltaz) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	//again, the iOS implement probably uses Entity::move, but too mainstream
	bl_Entity_setPos_helper(entity, entity->x + deltax, entity->y + deltay, entity->z + deltaz);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetRot
  (JNIEnv *env, jclass clazz, jlong entityId, jfloat yaw, jfloat pitch) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	bl_Entity_setRot(entity, yaw, pitch);
}

JNIEXPORT jfloat JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetPitch
  (JNIEnv *env, jclass clazz, jlong entityId) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return 0.0f;
	return entity->pitch;
}

JNIEXPORT jfloat JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetYaw
  (JNIEnv *env, jclass clazz, jlong entityId) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return 0.0f;
	return entity->yaw;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetCarriedItem
  (JNIEnv *env, jclass clazz, jlong entityId, jint itemId, jint itemCount, jint itemDamage) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	void* vtableEntry = entity->vtable[ENTITY_VTABLE_OFFSET_GET_CARRIED_ITEM];
	ItemInstance* (*fn)(Entity*) = (ItemInstance* (*) (Entity*)) vtableEntry;
	ItemInstance* item = fn(entity);
	if (item == NULL) return;
	item->count = itemCount;
	item->damage = itemDamage;
	bl_ItemInstance_setId(item, itemId);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetFov
  (JNIEnv *env, jclass clazz, jfloat newfov, jboolean override) {
	bl_newfov = newfov;
	if (override) {
		memcpy((void*) ((uintptr_t) bl_marauder_translation_function(bl_GameRenderer_getFov) & ~1),
			getFovHooked, sizeof(getFovHooked));
	} else {
		memcpy((void*) ((uintptr_t) bl_marauder_translation_function(bl_GameRenderer_getFov) & ~1),
			getFovOriginal, sizeof(getFovOriginal));
	}
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeOnGraphicsReset
  (JNIEnv *env, jclass clazz) {
	//bl_NinecraftApp_onGraphicsReset(bl_minecraft);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetMobSkin
  (JNIEnv *env, jclass clazz, jlong entityId, jstring skinPath) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	//skins
	const char * skinUtfChars = (*env)->GetStringUTFChars(env, skinPath, NULL);
	bl_changeEntitySkin((void*) entity, skinUtfChars);
	(*env)->ReleaseStringUTFChars(env, skinPath, skinUtfChars);
}

JNIEXPORT jfloat JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetEntityLoc
  (JNIEnv *env, jclass clazz, jlong entityId, jint axis) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return 0;
	switch (axis) {
		case AXIS_X:
			return entity->x;
		case AXIS_Y:
			return entity->y;
		case AXIS_Z:
			return entity->z;
		default:
			return 0;
	}
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeRemoveEntity
(JNIEnv *env, jclass clazz, jlong entityId) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	bl_Entity_remove(entity); //yes, I know I probably need to call through the vtable
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetEntityTypeId
  (JNIEnv *env, jclass clazz, jlong entityId) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return 0;
	void* vtable = entity->vtable[ENTITY_VTABLE_OFFSET_GET_ENTITY_TYPE_ID];
	int (*fn)(Entity*) = (int (*) (Entity*)) vtable;
	return fn(entity) & 0xff;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetAnimalAge
  (JNIEnv *env, jclass clazz, jlong entityId, jint age) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	bl_AgebleMob_setAge(entity, age);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetAnimalAge
  (JNIEnv *env, jclass clazz, jlong entityId) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return 0;
	return ((int*) entity)[772];
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeClearSlotInventory
  (JNIEnv *env, jclass clazz, jint slot) {
	if (bl_localplayer == NULL) return;
	//we grab the inventory instance from the player
	void* invPtr = *((void**) (((intptr_t) bl_localplayer) + PLAYER_INVENTORY_OFFSET)); //TODO fix this for 0.7.2
	bl_FillingContainer_clearSlot(invPtr, slot);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetSlotInventory
  (JNIEnv *env, jclass clazz, jint slot, jint type) {
	if (bl_localplayer == NULL) return 0;
	//we grab the inventory instance from the player
	void* invPtr = *((void**) (((intptr_t) bl_localplayer) + PLAYER_INVENTORY_OFFSET)); //TODO fix this for 0.7.2
	ItemInstance* instance = bl_FillingContainer_getItem(invPtr, slot);
	if (instance == NULL) return 0;
	switch (type) {
		case ITEMID:
			return bl_ItemInstance_getId(instance);
		case DAMAGE:
			return instance->damage;
		case AMOUNT:
			return instance->count;
		default:
			return 0;
	}
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetInventorySlot
  (JNIEnv *env, jclass clazz, jint slot, jint id, jint count, jint damage) {
	if (bl_localplayer == NULL) return;
	//we grab the inventory instance from the player
	void* invPtr = *((void**) (((intptr_t) bl_localplayer) + PLAYER_INVENTORY_OFFSET)); //TODO Merge this into a macro
	ItemInstance* itemStack = bl_newItemInstance(id, count, damage);
	if (itemStack == NULL) return;
	bl_FillingContainer_replaceSlot(invPtr, slot, itemStack);
	free(itemStack);
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetGameSpeed
  (JNIEnv *env, jclass clazz, jfloat ticksPerSecond) {
	MCPETimer* timer = *((MCPETimer**) (((uintptr_t) bl_minecraft) + MINECRAFT_TIMER_OFFSET));
	timer->ticksPerSecond = ticksPerSecond;
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetSelectedSlotId
  (JNIEnv *env, jclass clazz) {
	if (bl_localplayer == NULL) return 0;
	void* invPtr = *((void**) (((intptr_t) bl_localplayer) + PLAYER_INVENTORY_OFFSET));
	if (invPtr == NULL) return 0;
	return ((int*) invPtr)[INVENTORY_SELECTED_SLOT_OFFSET];
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetMobHealth
  (JNIEnv *env, jclass clazz, jlong entityId) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return 0;
	return ((int*) entity)[MOB_HEALTH_OFFSET];
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetMobHealth
  (JNIEnv *env, jclass clazz, jlong entityId, jint halfhearts) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	((int*) entity)[MOB_HEALTH_OFFSET] = halfhearts;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetEntityRenderType
  (JNIEnv *env, jclass clazz, jlong entityId, jint renderType) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return;
	bl_renderManager_setRenderType(entity, renderType);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetSlotArmor
  (JNIEnv *env, jclass clazz, jint slot, jint type) {
	if (bl_localplayer == NULL) return 0;
	//Geting the item
	ItemInstance* instance = bl_Player_getArmor(bl_localplayer, slot);
	if(instance == NULL) return 0;
	switch (type)
	{
		case ITEMID:
			return bl_ItemInstance_getId(instance);
		case DAMAGE:
			return instance->damage;
		case AMOUNT:
			return instance->count;
		default:
			return 0;
	}
	return 0;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetArmorSlot
  (JNIEnv *env, jclass clazz, jint slot, jint id, jint damage) {
	if (bl_localplayer == NULL) return;
	//Geting the item
	ItemInstance* instance = bl_newItemInstance(id, 1, damage);
	bl_Player_setArmor(bl_localplayer, slot, instance);
}

JNIEXPORT jfloat JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetEntityVel
  (JNIEnv *env, jclass clazz, jlong entityId, jint axis) {
	Entity* entity = bl_getEntityWrapper(bl_level, entityId);
	if (entity == NULL) return 0;
	switch (axis) {
		case AXIS_X:
			return entity->motionX;
		case AXIS_Y:
			return entity->motionY;
		case AXIS_Z:
			return entity->motionZ;
		default:
			return 0;
	}
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeAddItemFurnace
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot, jint id, jint damage, jint amount) {
	if (bl_level == NULL) return;
	ItemInstance* instance = bl_newItemInstance(id, amount, damage);

	void* tileEnt = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	if (tileEnt == NULL) return;
	bl_FurnaceTileEntity_setItem(tileEnt, slot, instance);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetItemFurnace
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot) {
	if (bl_level == NULL) return -1;

	void* tileEnt = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	ItemInstance* instance = bl_FurnaceTileEntity_getItem(tileEnt, slot);
	if (tileEnt == NULL) return -1;
	return bl_ItemInstance_getId(instance);
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetItemDataFurnace
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot) {
	if (bl_level == NULL) return -1;

	void* tileEnt = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	if (tileEnt == NULL) return -1;
	ItemInstance* instance = bl_FurnaceTileEntity_getItem(tileEnt, slot);
	return instance->damage;
}

JNIEXPORT jint JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeGetItemCountFurnace
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint slot) {
	if (bl_level == NULL) return -1;

	void* tileEnt = bl_TileSource_getTileEntity(bl_level->tileSource, x, y, z);
	if (tileEnt == NULL) return -1;
	ItemInstance* instance = bl_FurnaceTileEntity_getItem(tileEnt, slot);
	return instance->count;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeRemoveItemBackground
  (JNIEnv *env, jclass clazz) {
	//void* ItemRenderer_renderGuiItem = dlsym(RTLD_DEFAULT, "_ZN12ItemRenderer13renderGuiItemEP4FontP8TexturesPK12ItemInstanceffffb");
	//int drawRedSquareInstrLoc = ((int) ItemRenderer_renderGuiItem & ~1) + (0x131b38 - 0x131aa8);
	//*((int*) drawRedSquareInstrLoc) = 0xbf00bf00; //NOP
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeRequestFrameCallback
  (JNIEnv *env, jclass clazz) {
	bl_frameCallbackRequested = 1;
}

extern void bl_signalhandler_init();
extern void bl_cape_init(void*);

static void setupIsModded() {
#ifdef __arm__
	uintptr_t isModdedAddr = ((uintptr_t) bl_marauder_translation_function(
		dobby_dlsym(mcpelibhandle, "_ZN9Minecraft8isModdedEv"))) & ~1;
	unsigned char* isModdedArray = (unsigned char*) isModdedAddr;
	isModdedArray[0] = 1;
#endif
}

static bool exitEnabled = true;

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetExitEnabled
  (JNIEnv* env, jclass clazz, jboolean p) {
	exitEnabled = p;
}

static void (*App_quit_real)(void*);
static void App_quit_hook(void* self) {
	__android_log_print(ANDROID_LOG_INFO, "BlockLauncher", "app quit");
	if (exitEnabled) {
		App_quit_real(self);
	}
}

#include "checktamper.h"

void bl_prepatch_cppside(void*);

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativePrePatch
  (JNIEnv *env, jclass clazz, jboolean signalhandler, jobject activity) {
	if (bl_hasinit_prepatch) return;
#ifndef __i386
	if (signalhandler) bl_signalhandler_init();
#endif
	checkTamper(env, activity);
	if (!mcpelibhandle) {
		mcpelibhandle = (soinfo2*) dlopen("libminecraftpe.so", RTLD_LAZY);
	}
	void* readAssetFile = (void*) dobby_dlsym(mcpelibhandle, "_ZN19AppPlatform_android13readAssetFileERKSs");
	void* readAssetFileToHook = (void*) dobby_dlsym(mcpelibhandle, "_ZN21AppPlatform_android2313readAssetFileERKSs");
	void* tempPtr;
	mcpelauncher_hook(readAssetFileToHook, readAssetFile, &tempPtr);
	void** minecraftVtable = (void**) dobby_dlsym(mcpelibhandle, "_ZTV15MinecraftClient");
	App_quit_real = minecraftVtable[MINECRAFT_VTABLE_OFFSET_QUIT];
	minecraftVtable[MINECRAFT_VTABLE_OFFSET_QUIT] = &App_quit_hook;
	setupIsModded();
	//void** appPlatformVtable = (void**) dobby_dlsym(mcpelibhandle, "_ZTV21AppPlatform_android23");
	//replace the native code read asset method with the old one that went through JNI
	//appPlatformVtable[APPPLATFORM_VTABLE_OFFSET_READ_ASSET_FILE] = NULL;
/*
	void* humanoidModel_constructor = dlsym(mcpelibhandle, "_ZN13HumanoidModelC1Eff");
	__android_log_print(ANDROID_LOG_INFO, "BlockLauncher", "Hooking: %x", ((unsigned int) humanoidModel_constructor) - mcpelibhandle->base);
	mcpelauncher_hook(humanoidModel_constructor, (void*) &bl_HumanoidModel_constructor_hook, (void**) &bl_HumanoidModel_constructor_real);
	void* enderManModel_constructor = dlsym(mcpelibhandle, "_ZN13EnderManModelC1Ev");
	mcpelauncher_hook(enderManModel_constructor, (void*) &bl_EnderManModel_constructor_hook,
		(void**) &bl_EnderManModel_constructor_real);
*/

	bl_ModelPart_addBox = dlsym(mcpelibhandle, "_ZN9ModelPart6addBoxEfffiiif");

	jclass clz = (*env)->FindClass(env, "net/zhuoweizhang/mcpelauncher/ScriptManager");

	bl_scriptmanager_class = (*env)->NewGlobalRef(env, clz);
	//get a callback when the level is exited
	void* leaveGame = dlsym(RTLD_DEFAULT, "_ZN9Minecraft9leaveGameEb");
	mcpelauncher_hook(leaveGame, &bl_Minecraft_leaveGame_hook, (void**) &bl_Minecraft_leaveGame_real);
	bl_prepatch_cppside(mcpelibhandle);
	bl_hasinit_prepatch = 1;
}

void bl_dumpVtable(void** vtable, size_t size) {
	// thanks, MrARM
	Dl_info info;
	for (int i = 0; i < (size / sizeof(void*)); i++) {
		if (!dladdr(vtable[i], &info)) continue;
		__android_log_print(ANDROID_LOG_INFO, "BlockLauncher", "%d: %s", i, info.dli_sname);
	}
}
int bl_findVtable(void** vtable, void* needle) {
	int i = 0;
	while (vtable[i] != needle) i++;
	return i;
}

JNIEXPORT void JNICALL Java_net_zhuoweizhang_mcpelauncher_ScriptManager_nativeSetupHooks
  (JNIEnv *env, jclass clazz, jint versionCode) {
	if (bl_hasinit_script) return;

	dlerror();

	if (!mcpelibhandle) {
		mcpelibhandle = (soinfo2*) dlopen("libminecraftpe.so", RTLD_LAZY);
	}

	//edit the vtables of the GameMode implementations
	bl_GameMode_useItemOn_real = dlsym(RTLD_DEFAULT, "_ZN8GameMode9useItemOnER6PlayerP12ItemInstanceRK7TilePosaRK4Vec3");

	void** creativeVtable = (void**) dobby_dlsym(mcpelibhandle, "_ZTV12CreativeMode");
	creativeVtable[GAMEMODE_VTABLE_OFFSET_USE_ITEM_ON] = (void*) &bl_GameMode_useItemOn_hook;
	void** survivalVtable = (void**) dobby_dlsym(mcpelibhandle, "_ZTV12SurvivalMode");
	survivalVtable[GAMEMODE_VTABLE_OFFSET_USE_ITEM_ON] = (void*) &bl_GameMode_useItemOn_hook;

	bl_GameMode_attack_real = dlsym(RTLD_DEFAULT, "_ZN8GameMode6attackEP6PlayerP6Entity");
	creativeVtable[GAMEMODE_VTABLE_OFFSET_ATTACK] = (void*) &bl_GameMode_attack_hook;
	survivalVtable[GAMEMODE_VTABLE_OFFSET_ATTACK] = (void*) &bl_GameMode_attack_hook;
	bl_GameMode_tick_real = dlsym(RTLD_DEFAULT, "_ZN8GameMode4tickEv");
	creativeVtable[GAMEMODE_VTABLE_OFFSET_TICK] = (void*) &bl_GameMode_tick_hook;
	survivalVtable[GAMEMODE_VTABLE_OFFSET_TICK] = (void*) &bl_GameMode_tick_hook;

	bl_GameMode_initPlayer_real = dlsym(RTLD_DEFAULT, "_ZN8GameMode10initPlayerEP6Player");
	creativeVtable[GAMEMODE_VTABLE_OFFSET_INIT_PLAYER] = (void*) &bl_GameMode_initPlayer_hook;
	survivalVtable[GAMEMODE_VTABLE_OFFSET_INIT_PLAYER] = (void*) &bl_GameMode_initPlayer_hook;

	//edit the vtable of NinecraftApp to get a callback when levels are switched
	void** minecraftVtable = (void**) dobby_dlsym(mcpelibhandle, "_ZTV15MinecraftClient");
	//void** levelVtable = (void**) dobby_dlsym(mcpelibhandle, "_ZTV5Level");
	bl_dumpVtable((void**) minecraftVtable, 0x100);
	bl_Minecraft_setLevel_real = minecraftVtable[MINECRAFT_VTABLE_OFFSET_SET_LEVEL];

	minecraftVtable[MINECRAFT_VTABLE_OFFSET_SET_LEVEL] = (void*) &bl_Minecraft_setLevel_hook;

	void* selectLevel = dlsym(RTLD_DEFAULT, "_ZN9Minecraft11selectLevelERKSsS1_RK13LevelSettings");
#ifndef __i386
	mcpelauncher_hook(selectLevel, &bl_Minecraft_selectLevel_hook, (void**) &bl_Minecraft_selectLevel_real);
#endif

	void* destroyBlock = dlsym(RTLD_DEFAULT, "_ZN8GameMode12destroyBlockEP6Playeriiia");
	mcpelauncher_hook(destroyBlock, &bl_GameMode_destroyBlock_hook, (void**) &bl_GameMode_destroyBlock_real);
	
	void* startDestroyBlockSurvival = dlsym(RTLD_DEFAULT, "_ZN12SurvivalMode17startDestroyBlockEP6Playeriiia");
	mcpelauncher_hook(startDestroyBlockSurvival, &bl_SurvivalMode_startDestroyBlock_hook, (void**) &bl_SurvivalMode_startDestroyBlock_real);

	void* startDestroyBlockCreative = dlsym(RTLD_DEFAULT, "_ZN12CreativeMode17startDestroyBlockEP6Playeriiia");
	mcpelauncher_hook(startDestroyBlockCreative, &bl_CreativeMode_startDestroyBlock_hook, (void**) &bl_CreativeMode_startDestroyBlock_real);

	void* getFov = dlsym(RTLD_DEFAULT, "_ZN12GameRenderer6getFovEfb");
	memcpy(getFovOriginal, (void*) ((uintptr_t) getFov & ~1), sizeof(getFovOriginal));
	mcpelauncher_hook(getFov, &bl_GameRenderer_getFov_hook, (void**) &bl_GameRenderer_getFov_real);
	memcpy(getFovHooked, (void*) ((uintptr_t) getFov & ~1), sizeof(getFovHooked));
	// start off with original FOV
	memcpy((void*) ((uintptr_t) bl_marauder_translation_function(getFov) & ~1), getFovOriginal, sizeof(getFovOriginal));
	bl_GameRenderer_getFov = getFov;

	//get the level set block method. In future versions this might link against libminecraftpe itself
	bl_TileSource_setTileAndData = dlsym(RTLD_DEFAULT, "_ZN10TileSource14setTileAndDataEiii8FullTilei");
	if (bl_TileSource_setTileAndData == NULL) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Failed to get setTileAndData: %s\n", dlerror());
	}

	bl_TileSource_getData = dlsym(RTLD_DEFAULT, "_ZN10TileSource7getDataEiii");

	bl_Player_getCarriedItem = dlsym(RTLD_DEFAULT, "_ZN6Player14getCarriedItemEv");
	// FIXME!
	bl_Entity_setPos = dlsym(RTLD_DEFAULT, "_ZN6Entity6setPosEfff");
	bl_Level_explode = dlsym(RTLD_DEFAULT, "_ZN5Level7explodeEP6Entityffffb");
	bl_Inventory_add = dlsym(RTLD_DEFAULT, "_ZN9Inventory3addEP12ItemInstance");
	//bl_MobFactory_getStaticTestMob = dlsym(RTLD_DEFAULT, "_ZN10MobFactory16getStaticTestMobEiP5Level");

	bl_TileSource_getRawBrightness = dlsym(RTLD_DEFAULT, "_ZN10TileSource16getRawBrightnessEiiib");
	bl_Level_setNightMode = dlsym(RTLD_DEFAULT, "_ZN5Level12setNightModeEb");
	bl_Entity_setRot = dlsym(RTLD_DEFAULT, "_ZN6Entity6setRotEff");
	//bl_NinecraftApp_onGraphicsReset = dlsym(RTLD_DEFAULT, "_ZN12NinecraftApp15onGraphicsResetEv");
	bl_Mob_getTexture = dlsym(RTLD_DEFAULT, "_ZN3Mob10getTextureEv");
	bl_LocalPlayer_hurtTo = dlsym(RTLD_DEFAULT, "_ZN11LocalPlayer6hurtToEi");
	bl_Entity_remove = dlsym(RTLD_DEFAULT, "_ZN6Entity6removeEv");
	bl_Entity_spawnAtLocation = dlsym(RTLD_DEFAULT, "_ZN6Entity15spawnAtLocationERK12ItemInstancef");
	bl_Level_setTime = dlsym(RTLD_DEFAULT, "_ZN5Level7setTimeEl");
	bl_Level_getTime = dlsym(RTLD_DEFAULT, "_ZNK5Level7getTimeEv");
	bl_Level_getLevelData = dlsym(RTLD_DEFAULT, "_ZN5Level12getLevelDataEv");
	bl_LevelData_setSpawn = dlsym(RTLD_DEFAULT, "_ZN9LevelData8setSpawnERK7TilePos");
	bl_LevelData_setGameType = dlsym(RTLD_DEFAULT, "_ZN9LevelData11setGameTypeEi");
	bl_LevelData_getGameType = dlsym(RTLD_DEFAULT, "_ZNK9LevelData11getGameTypeEv");
	bl_TileSource_getTileEntity = dlsym(RTLD_DEFAULT, "_ZN10TileSource13getTileEntityEiii");
	bl_ChestTileEntity_setItem = dlsym(RTLD_DEFAULT, "_ZN15ChestTileEntity7setItemEiP12ItemInstance");
	bl_ChestTileEntity_getItem = dlsym(RTLD_DEFAULT, "_ZNK15ChestTileEntity7getItemEi");
	bl_FurnaceTileEntity_setItem = dlsym(RTLD_DEFAULT, "_ZN17FurnaceTileEntity7setItemEiP12ItemInstance");
	bl_FurnaceTileEntity_getItem = dlsym(RTLD_DEFAULT, "_ZNK17FurnaceTileEntity7getItemEi");
	bl_Entity_setOnFire = dlsym(RTLD_DEFAULT, "_ZN6Entity9setOnFireEi");
	bl_FillingContainer_clearSlot = dlsym(RTLD_DEFAULT, "_ZN16FillingContainer9clearSlotEi");
	bl_FillingContainer_getItem = dlsym(RTLD_DEFAULT, "_ZNK16FillingContainer7getItemEi");
	bl_Player_getArmor = dlsym(RTLD_DEFAULT, "_ZN6Player8getArmorEi");
	bl_Player_setArmor = dlsym(RTLD_DEFAULT, "_ZN6Player8setArmorEiPK12ItemInstance");

	//replace the getTexture method for zombie pigmen
	void** pigZombieVtable = (void**) dobby_dlsym(mcpelibhandle, "_ZTV9PigZombie");
	//pigZombieVtable[MOB_VTABLE_OFFSET_GET_TEXTURE] = (void*) bl_Mob_getTexture;

	bl_AgebleMob_setAge = dlsym(RTLD_DEFAULT, "_ZN9AgableMob6setAgeEi");
	bl_Minecraft_setIsCreativeMode = dlsym(RTLD_DEFAULT, "_ZN9Minecraft17setIsCreativeModeEb");
	//bl_Inventory_clearInventoryWithDefault = dlsym(RTLD_DEFAULT, "_ZN9Inventory25clearInventoryWithDefaultEv");
	bl_Inventory_Inventory = dlsym(RTLD_DEFAULT, "_ZN9InventoryC2EP6Playerb");
	bl_Inventory_delete1_Inventory = dlsym(RTLD_DEFAULT, "_ZN9InventoryD1Ev");
	bl_ItemInstance_setId = dlsym(RTLD_DEFAULT, "_ZN12ItemInstance8_setItemEi"); //note the name change: consistent naming
	bl_ItemInstance_getId = dlsym(RTLD_DEFAULT, "_ZNK12ItemInstance5getIdEv");
	//replace the update method in Minecraft with our own
	bl_NinecraftApp_update_real = minecraftVtable[MINECRAFT_VTABLE_OFFSET_UPDATE];
#if 0
	for (int i = 0; i < 40; i++) {
		if (minecraftVtable[i] == (int) bl_NinecraftApp_update_real) {
			__android_log_print(ANDROID_LOG_INFO, LOG_TAG, "vtable: %d", i);
		}
	}
#endif

	minecraftVtable[MINECRAFT_VTABLE_OFFSET_UPDATE] = &bl_NinecraftApp_update_hook;

	bl_FillingContainer_replaceSlot = dlsym(mcpelibhandle, "_ZN16FillingContainer11replaceSlotEiP12ItemInstance");
	bl_LevelRenderer_allChanged = dlsym(mcpelibhandle, "_ZN13LevelRenderer10allChangedEv");
	bl_FillingContainer_removeResource = dlsym(mcpelibhandle, "_ZN16FillingContainer14removeResourceERK12ItemInstanceb");

	bl_setuphooks_cppside();

	bl_cape_init(mcpelibhandle);

	bl_hasinit_script = 1;

	const char* myerror = dlerror();
	if (myerror != NULL) {
		__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Error in ModPE script init: %s\n", myerror);
	}
	__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Size of ModelPart: %d", sizeof(ModelPart));

}
