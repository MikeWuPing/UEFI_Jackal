/* JACKAL_RAM：2KB NES RAM 镜像（union 双视图）。
   字段名贴 RAM_Symbols.ASM 汇编符号，偏移必须等于 NES 地址——_Static_assert 机械保证；
   "duplicate for ease of reading" 别名用 #define，不重复占存储。
   Label844（Bank7.ASM:1387-1409）清零 $23-$DF 与 $0300-$06FF 后以 JSR InitPPU 收尾，
   定义随 Task 2.5 落在 loader.c（与 InitPPU 同文件）；本头只声明。 */
#ifndef JACKAL_CORE_RAM_H
#define JACKAL_CORE_RAM_H

#include <stddef.h>
#include <stdint.h>

/* MSVC C 模式把 C11 匿名 struct/union 成员当扩展（C4201），/WX 下致命；pragma 就地豁免 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4201)
#endif

typedef union {
  uint8_t Raw[0x800];
  struct {
    uint8_t pad0000[0x18];
    uint8_t GameControlState;        /* $0018 */
    uint8_t ControlSubState;         /* $0019 TitleScreenState/StageTransitionState 等复用 */
    uint8_t RNG_INCEveryFrame;       /* $001A */
    uint8_t GameFrameLogicInProgress;/* $001B */
    uint8_t TitleScreenMode;         /* $001C */
    uint8_t PlayerMode_1or2;         /* $001D tblPlayerMode：单人 $01/双人 $07（bit2=双人） */
    uint8_t pad001E[2];
    uint8_t ScreenTransitionTimer;   /* $0020 */
    uint8_t PPUGraphicsUpdateByteLength; /* $0021 */
    uint8_t Player2Active;           /* $0022 */
    uint8_t SpriteSlotRotation;      /* $0023 */
    uint8_t pad0024;
    uint8_t GamePaused;              /* $0025 */
    uint8_t pad0026;
    uint8_t CurrentBank;             /* $0027 */
    uint8_t pad0028[2];
    uint8_t ScreenTimerLB;           /* $002A */
    uint8_t ScreenTimerUB;           /* $002B */
    uint8_t GamePlayMode;            /* $002C */
    uint8_t pad002D[3];
    uint8_t CurrentLevel;            /* $0030 zero-based */
    uint8_t Jeep1LifeCount;          /* $0031 */
    uint8_t Jeep2LifeCount;          /* $0032 */
    uint8_t DifficultyBasedOnWeapon; /* $0033 */
    uint8_t ContinuesUsed;           /* $0034 通关置 $80 → 二周目强制最高难度 */
    uint8_t pad0035;
    uint8_t SpawnBlockIndex;         /* $0036 Label978 spawn 块内游标（汇编裸地址 $36） */
    uint8_t LastSpawnedEnemyY_HB;    /* $0037 */
    uint8_t LastSpawnedEnemyY_LB;    /* $0038 */
    uint8_t pad0039;
    uint8_t ScreenVerticalScrollLockForBossFight; /* $003A */
    uint8_t LevelBossEntitiesRemaining;    /* $003B 为 0 时持续检测关卡结束 */
    uint8_t ScreenScrollingForF0ToBoss;    /* $003C */
    uint8_t PreviousScreenDataScrollTracking; /* $003D */
    uint8_t pad003E[2];
    uint8_t ScreenLeftScrollPosition;      /* $0040 */
    uint8_t pad0041;
    uint8_t EndofLevelDelayAfterBossDeath; /* $0042 */
    uint8_t CurrentLevelScreen;            /* $0043 1=关卡首屏 */
    uint8_t CurrentLevelScreenSubPosition; /* $0044 0-$EF，跨屏归零 */
    uint8_t PreviousLevelScreenSubposition;/* $0045 */
    uint8_t pad0046;
    uint8_t Zp47, Zp48, Zp49, Zp4A, Zp4B, Zp4C, Zp4D; /* $0047-$004D 滚屏列装载工作区 */
    uint8_t ScreenScrolledUp_Down;         /* $004E 0=向上滚（关卡推进，:5115）/1=向下回滚（:5130） */
    uint8_t Zp4F;                          /* $004F 滚屏工作区 */
    uint8_t JeepMainWeapon[2];       /* $0050-$0051（$50 在 Continue 画面复用为 YES/NO 光标） */
    uint8_t JeepIFrameTimer[2];      /* $0052-$0053（标题画面 $52 复用为 POWPortraitTextIndex） */
    uint8_t JeepPOWCount[2];         /* $0054-$0055 */
    uint8_t JeepHorizPosition[2];    /* $0056-$0057 屏幕像素 */
    uint8_t JeepVertPosition[2];     /* $0058-$0059 屏幕像素 */
    uint8_t JeepAtHelipadDropoff[2]; /* $005A-$005B */
    uint8_t JeepPOWDropoffDelay[2];  /* $005C-$005D */
    uint8_t pad005E[2];
    uint8_t JeepPOWHeliDropOffCount[2]; /* $0060-$0061 */
    uint8_t JeepDirectionTendency[2];   /* $0062-$0063 */
    uint8_t JeepFacingDirection[2];     /* $0064-$0065 */
    uint8_t JeepVisibleFrameTimer[2];   /* $0066-$0067 */
    uint8_t JeepEscalatorEffectActive[2]; /* $0068-$0069 */
    uint8_t SpriteAttributes;           /* $006A 精灵构造临时 */
    uint8_t pad006B[0x0F];
    uint8_t Jeep1Attributes;            /* $007A 镜像/调色板属性 */
    uint8_t Jeep2Attributes;            /* $007B */
    uint8_t JeepWeaponAttributes[0x0C]; /* $007C-$0087：子弹6($007C-$0081)+主武器2($0082-$0083)+splash4($0084-$0087) */
    uint8_t pad0088[4];                 /* $0088-$008B NOT_USED */
    uint8_t BazookaSplashHorizOffset[4];/* $008C-$008F */
    uint8_t pad0090[4];                 /* $0090-$0093 NOT_USED */
    uint8_t BazookaSplashVertOffset[4]; /* $0094-$0097 */
    uint8_t pad0098[0x12];              /* $0098-$00A9 */
    uint8_t EnemyPoints[0x10];          /* $00AA-$00B9 RAM_Symbols.ASM:199-214 */
    uint8_t PPUGraphicsUpdateTableIndex; /* $00BA */
    uint8_t PPUGraphicsUpdateComplete;   /* $00BB */
    uint8_t pad00BC[8];
    uint8_t HelipadLightFlashPalette;    /* $00C4 */
    uint8_t JeepNext1Up[2];              /* $00C5-$00C6 */
    uint8_t LevelHelipadLightFlashTimer; /* $00C7 */
    uint8_t pad00C8[0x0D];               /* $00C8-$00D4 */
    uint8_t Level6BossTurretStatus;      /* $00D5（RAM_Symbols.ASM:244 注：MSB=换 palette、
                                            LSB=存活——L6 boss 双激光炮塔/坦克炮塔共用；
                                            L5 boss 亦借作坦克计数 Bank6:5658） */
    uint8_t POWDropOffWalkDirection;     /* $00D6（RAM_Symbols.ASM:245：$01=LEFT/$FF=RIGHT） */
    uint8_t pad00D7;
    uint8_t JeepMovementDirection;       /* $00D8 兼分数/经验显示（RAM_Symbols.ASM:250） */
    uint8_t JeepMovementState;           /* $00D9 00=Moving/01=Water/02=Stationary（:251） */
    uint8_t pad00DA[0x17];               /* $00DA-$00F0 */
    uint8_t JeepControlsInput[2];        /* $00F1-$00F2 持续按住 */
    uint8_t JeepBButtonHeld[2];          /* $00F3-$00F4 */
    uint8_t JeepControlsInput1Frame[2];  /* $00F5-$00F6 本帧新按下（边沿） */
    uint8_t pad00F7[5];
    uint8_t ScreenVerticalScrollPosition_PPU;   /* $00FC 上行递减，$EF 回绕 */
    uint8_t ScreenHorizontalScrollPosition_PPU; /* $00FD */
    uint8_t NormalPPUMASK;                      /* $00FE */
    uint8_t NormalPPUCTRL;                      /* $00FF */
    uint8_t pad0100[0x10];
    uint8_t LevelBGAnimatedTileUpdateTimer; /* $0110 */
    uint8_t LevelBGAnimatedTileUpdateIndex; /* $0111 */
    uint8_t Level6FinalBossExplosionTableIndex; /* $0112 爆炸位置表游标（:8419） */
    uint8_t Level6FinalBossBuildingBlowingUpTimeUntilTankSpawns; /* $0113 */
    uint8_t Level6FinalBossBuildingBlowingUpTimeUntilFinalBossMusicStarts; /* $0114 */
    uint8_t pad0115;
    uint8_t Level6FinalBossDefeated_PPUUpdate_SoundClipInitiated; /* $0116 建筑/坦克共用 */
    uint8_t Level6FinalBossCurrentGraphics_PaletteUpdateIndex; /* $0117 */
    uint8_t Level6FinalBossEndingGraphics_PaletteUpdateIndex;  /* $0118 到顶停（不装载） */
    uint8_t Level6FinalBossFreezePlayerJeep_InvulnerableWhileExploding; /* $0119 */
    uint8_t pad011A[0xE6];
    uint8_t OamShadow[0x100];   /* $0200-$02FF OAM shadow（Phase 3 填充） */
    uint8_t pad0300[0x200];     /* $0300-$04FF 工作区（Label844 清零范围） */
    uint8_t SpriteTypeIndex[0x20];              /* $0500 */
    uint8_t SpriteState[0x20];                  /* $0520 */
    uint8_t SpriteGraphicsAttributes[0x20];     /* $0540 */
    uint8_t SpriteVertScreenPosition[0x20];     /* $0560 */
    uint8_t SpriteVertScreenPositionSubPixel[0x20]; /* $0580 */
    uint8_t SpriteHorizScreenPosition[0x20];    /* $05A0 */
    uint8_t SpriteHorizScreenPositionSubPixel[0x20]; /* $05C0 */
    uint8_t SpriteVertSpeedUB[0x20];            /* $05E0 */
    uint8_t SpriteVertSpeedLB[0x20];            /* $0600 */
    uint8_t SpriteHorizSpeedUB[0x20];           /* $0620 */
    uint8_t SpriteHorizSpeedLB[0x20];           /* $0640 */
    uint8_t SpriteData1[0x10];                  /* $0660 六数组各 $10（RAM_Symbols.ASM:1772-1871，仅 16 槽无吉普段） */
    uint8_t SpriteData2[0x10];                  /* $0670 */
    uint8_t SpriteData3[0x10];                  /* $0680 */
    uint8_t SpriteData4[0x10];                  /* $0690 */
    uint8_t SpriteData5[0x10];                  /* $06A0 */
    uint8_t SpriteData6[0x10];                  /* $06B0 */
    uint8_t SpriteWhatDirectionToShoot[0x10];   /* $06C0 仅 16 槽（无吉普别名段） */
    uint8_t SpriteData8[0x10];                  /* $06D0 */
    uint8_t SpriteWhichJeeptoAttack[0x10];      /* $06E0 */
    uint8_t SpriteAbsoluteHorizPositionUB[0x10];/* $06F0 */
    uint8_t SpriteAbsoluteVertPositionUB[0x10]; /* $0700 */
    uint8_t SpriteAbsoluteHorizPositionLB[0x10];/* $0710 */
    uint8_t SpriteObjectID[0x10];               /* $0720 */
    uint8_t SpriteHitboxShapeIndex[0x10];       /* $0730 */
    uint8_t SpriteHealthHP[0x10];               /* $0740 */
    uint8_t JeepBulletDisplayFrames[6];         /* $0750-$0755 P1三发+P2三发（RAM_Symbols.ASM:2038-2043） */
    uint8_t pad0756[6];                         /* $0756-$075B */
    uint8_t JeepMainWeaponTimer[2];             /* $075C-$075D（:2052-2054） */
    uint8_t pad075E[0x0E];                      /* $075E-$076B */
    uint8_t SoundUpdateInProgress;        /* $076C 声音 stub 状态 */
    uint8_t Level6BossTankScroll_Next;    /* $076D */
    uint8_t Level6BossTankScroll_Current; /* $076E */
    uint8_t pad076F;
    uint8_t PPUUpdateQueue[0x70]; /* $0770-$07DF PPUCTRLValue/PPUAddressUB/LB/PPUData0-74 */
    uint8_t HighScore[3];         /* $07E0-$07E2 BCD 三字节 */
    uint8_t pad07E3;
    uint8_t Jeep1Score[3];        /* $07E4-$07E6 */
    uint8_t pad07E7;
    uint8_t Jeep2Score[3];        /* $07E8-$07EA */
    uint8_t pad07EB[0x15];
  };
} JACKAL_RAM;

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/* 别名（RAM_Symbols.ASM "duplicate for ease of reading"）：不占存储 */
#define JeepLifeCount          Jeep1LifeCount
#define JeepAttributes         Jeep1Attributes   /* RAM_Symbols.ASM:142（$007A） */
#define POWPortraitTextIndex   JeepIFrameTimer[0]
#define Jeep1HorizPosition     JeepHorizPosition[0]
#define Jeep2HorizPosition     JeepHorizPosition[1]
#define Jeep1VertPosition      JeepVertPosition[0]
#define Jeep2VertPosition      JeepVertPosition[1]
#define Jeep1Next1Up           JeepNext1Up[0]    /* RAM_Symbols.ASM:227（$00C5） */
#define Jeep2Next1Up           JeepNext1Up[1]    /* $00C6 */
#define Jeep1POWHeliDropOffCount JeepPOWHeliDropOffCount[0]  /* $0060 */
#define Jeep2POWHeliDropOffCount JeepPOWHeliDropOffCount[1]  /* $0061 */
#define JeepTypeIndex          SpriteTypeIndex[0x10]
#define Jeep2TypeIndex         SpriteTypeIndex[0x11]
#define Jeep1GraphicsAttributes SpriteGraphicsAttributes[0x10]
#define Jeep2GraphicsAttributes SpriteGraphicsAttributes[0x11]
#define JeepVertScreenPosition  SpriteVertScreenPosition[0x10]
#define Jeep2VertScreenPosition SpriteVertScreenPosition[0x11]
#define JeepHorizScreenPosition  SpriteHorizScreenPosition[0x10]
#define Jeep2HorizScreenPosition SpriteHorizScreenPosition[0x11]
#define Jeep1EscalatorEffectActive JeepEscalatorEffectActive[0]  /* RAM_Symbols.ASM:123（$0068） */
#define Jeep2EscalatorEffectActive JeepEscalatorEffectActive[1]  /* $0069 */
#define Jeep1State             SpriteState[0x10]    /* RAM_Symbols.ASM:1423（$0530） */
#define Jeep2State             SpriteState[0x11]    /* $0531 */

/* 吉普子弹/主武器/splash = sprite 数组下标别名（子弹槽 $12-$17、主武器 $18-$19、splash $1A-$1D） */
#define JeepBulletTypeIndex      SpriteTypeIndex[0x12]      /* $0512 RAM_Symbols.ASM:1381 */
#define JeepMainWeaponTypeIndex  SpriteTypeIndex[0x18]      /* $0518 :1387 */
#define JeepBulletState          SpriteState[0x12]          /* $0532 :1425 */
#define JeepMainWeaponState      SpriteState[0x18]          /* $0538 :1431 */
#define JeepHorizBazookaSplashState  SpriteState[0x1A]      /* $053A :1434 */
#define JeepVerticalBazookaSplashState SpriteState[0x1C]    /* $053C :1436 */
#define JeepBulletGraphicsAttributes SpriteGraphicsAttributes[0x12]     /* $0552 :1470 */
#define JeepMainWeaponGraphicsAttributes SpriteGraphicsAttributes[0x18] /* $0558 :1476 */
#define JeepBulletVertScreenPosition SpriteVertScreenPosition[0x12]     /* $0572 :1506 */
#define JeepMainWeaponVertScreenPosition SpriteVertScreenPosition[0x18] /* $0578 :1512 */
#define JeepBulletHorizScreenPosition SpriteHorizScreenPosition[0x12]   /* $05B2 :1577 */
#define JeepMainWeaponHorizScreenPosition SpriteHorizScreenPosition[0x18] /* $05B8 :1583 */
#define JeepBulletVertSpeedUB    SpriteVertSpeedUB[0x12]    /* $05F2 :1648 */
#define JeepMainWeaponVertSpeedUB SpriteVertSpeedUB[0x18]   /* $05F8 :1654 */
#define JeepBulletVertSpeedLB    SpriteVertSpeedLB[0x12]    /* $0612 :1684 */
#define JeepMainWeaponVertSpeedLB SpriteVertSpeedLB[0x18]   /* $0618 :1690 */
#define JeepBulletHorizSpeedUB   SpriteHorizSpeedUB[0x12]   /* $0632 :1720 */
#define JeepMainWeaponHorizSpeedUB SpriteHorizSpeedUB[0x18] /* $0638 :1726 */
#define JeepBulletHorizSpeedLB   SpriteHorizSpeedLB[0x12]   /* $0652 :1756 */
#define JeepMainWeaponHorizSpeedLB SpriteHorizSpeedLB[0x18] /* $0658 :1762 */
#define JeepBulletAttributes     JeepWeaponAttributes[0]    /* $007C :145（+0..5 六发槽） */
#define JeepMainWeaponAttributes JeepWeaponAttributes[6]    /* $0082 :151（+0..1 双吉普） */
#define JeepHorizBazookaSplashHorizOffset BazookaSplashHorizOffset[0] /* $008C :166 */
#define JeepHorizBazookaSplashVertOffset  BazookaSplashVertOffset[0]  /* $0094 :176 */

/* 布局断言：偏移必须等于 NES 地址。
   宿主 cl 用 /std:c11（_Static_assert 可用）；EDK2 默认 C 模式无 /std:c11，
   VS2019 在该模式不识别 _Static_assert → 回退 C89 负数组 typedef 技巧（同 EDK2 STATIC_ASSERT 思路）。 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#define RAM_ASSERT(field, addr) _Static_assert(offsetof(JACKAL_RAM, field) == (addr), #field)
#define RAM_ASSERT_SIZE(size)   _Static_assert(sizeof(JACKAL_RAM) == (size), "JACKAL_RAM size")
#else
#define RAM_ASSERT(field, addr) typedef char JackalRamAssert_##field[(offsetof(JACKAL_RAM, field) == (addr)) ? 1 : -1]
#define RAM_ASSERT_SIZE(size)   typedef char JackalRamAssert_sizeof[(sizeof(JACKAL_RAM) == (size)) ? 1 : -1]
#endif
RAM_ASSERT(GameControlState, 0x18);
RAM_ASSERT(ControlSubState, 0x19);
RAM_ASSERT(RNG_INCEveryFrame, 0x1A);
RAM_ASSERT(GameFrameLogicInProgress, 0x1B);
RAM_ASSERT(TitleScreenMode, 0x1C);
RAM_ASSERT(PlayerMode_1or2, 0x1D);
RAM_ASSERT(ScreenTransitionTimer, 0x20);
RAM_ASSERT(PPUGraphicsUpdateByteLength, 0x21);
RAM_ASSERT(Player2Active, 0x22);
RAM_ASSERT(SpriteSlotRotation, 0x23);
RAM_ASSERT(GamePaused, 0x25);
RAM_ASSERT(CurrentBank, 0x27);
RAM_ASSERT(ScreenTimerLB, 0x2A);
RAM_ASSERT(ScreenTimerUB, 0x2B);
RAM_ASSERT(GamePlayMode, 0x2C);
RAM_ASSERT(CurrentLevel, 0x30);
RAM_ASSERT(Jeep1LifeCount, 0x31);
RAM_ASSERT(Jeep2LifeCount, 0x32);
RAM_ASSERT(DifficultyBasedOnWeapon, 0x33);
RAM_ASSERT(ContinuesUsed, 0x34);
RAM_ASSERT(SpawnBlockIndex, 0x36);
RAM_ASSERT(LastSpawnedEnemyY_HB, 0x37);
RAM_ASSERT(LastSpawnedEnemyY_LB, 0x38);
RAM_ASSERT(ScreenVerticalScrollLockForBossFight, 0x3A);
RAM_ASSERT(LevelBossEntitiesRemaining, 0x3B);
RAM_ASSERT(ScreenScrollingForF0ToBoss, 0x3C);
RAM_ASSERT(PreviousScreenDataScrollTracking, 0x3D);
RAM_ASSERT(ScreenLeftScrollPosition, 0x40);
RAM_ASSERT(EndofLevelDelayAfterBossDeath, 0x42);
RAM_ASSERT(CurrentLevelScreen, 0x43);
RAM_ASSERT(CurrentLevelScreenSubPosition, 0x44);
RAM_ASSERT(PreviousLevelScreenSubposition, 0x45);
RAM_ASSERT(Zp47, 0x47);
RAM_ASSERT(Zp4D, 0x4D);
RAM_ASSERT(ScreenScrolledUp_Down, 0x4E);
RAM_ASSERT(Zp4F, 0x4F);
RAM_ASSERT(JeepMainWeapon, 0x50);
RAM_ASSERT(JeepIFrameTimer, 0x52);
RAM_ASSERT(JeepPOWCount, 0x54);
RAM_ASSERT(JeepHorizPosition, 0x56);
RAM_ASSERT(JeepVertPosition, 0x58);
RAM_ASSERT(JeepAtHelipadDropoff, 0x5A);
RAM_ASSERT(JeepPOWDropoffDelay, 0x5C);
RAM_ASSERT(JeepPOWHeliDropOffCount, 0x60);
RAM_ASSERT(JeepDirectionTendency, 0x62);
RAM_ASSERT(JeepFacingDirection, 0x64);
RAM_ASSERT(JeepVisibleFrameTimer, 0x66);
RAM_ASSERT(JeepEscalatorEffectActive, 0x68);
RAM_ASSERT(SpriteAttributes, 0x6A);
RAM_ASSERT(Jeep1Attributes, 0x7A);
RAM_ASSERT(Jeep2Attributes, 0x7B);
RAM_ASSERT(JeepWeaponAttributes, 0x7C);
RAM_ASSERT(BazookaSplashHorizOffset, 0x8C);
RAM_ASSERT(BazookaSplashVertOffset, 0x94);
RAM_ASSERT(EnemyPoints, 0xAA);
RAM_ASSERT(PPUGraphicsUpdateTableIndex, 0xBA);
RAM_ASSERT(PPUGraphicsUpdateComplete, 0xBB);
RAM_ASSERT(HelipadLightFlashPalette, 0xC4);
RAM_ASSERT(JeepNext1Up, 0xC5);
RAM_ASSERT(LevelHelipadLightFlashTimer, 0xC7);
RAM_ASSERT(Level6BossTurretStatus, 0xD5);
RAM_ASSERT(POWDropOffWalkDirection, 0xD6);
RAM_ASSERT(JeepMovementDirection, 0xD8);
RAM_ASSERT(JeepMovementState, 0xD9);
RAM_ASSERT(JeepControlsInput, 0xF1);
RAM_ASSERT(JeepBButtonHeld, 0xF3);
RAM_ASSERT(JeepControlsInput1Frame, 0xF5);
RAM_ASSERT(ScreenVerticalScrollPosition_PPU, 0xFC);
RAM_ASSERT(ScreenHorizontalScrollPosition_PPU, 0xFD);
RAM_ASSERT(NormalPPUMASK, 0xFE);
RAM_ASSERT(NormalPPUCTRL, 0xFF);
RAM_ASSERT(LevelBGAnimatedTileUpdateTimer, 0x110);
RAM_ASSERT(LevelBGAnimatedTileUpdateIndex, 0x111);
RAM_ASSERT(Level6FinalBossExplosionTableIndex, 0x112);
RAM_ASSERT(Level6FinalBossBuildingBlowingUpTimeUntilTankSpawns, 0x113);
RAM_ASSERT(Level6FinalBossBuildingBlowingUpTimeUntilFinalBossMusicStarts, 0x114);
RAM_ASSERT(Level6FinalBossDefeated_PPUUpdate_SoundClipInitiated, 0x116);
RAM_ASSERT(Level6FinalBossCurrentGraphics_PaletteUpdateIndex, 0x117);
RAM_ASSERT(Level6FinalBossEndingGraphics_PaletteUpdateIndex, 0x118);
RAM_ASSERT(Level6FinalBossFreezePlayerJeep_InvulnerableWhileExploding, 0x119);
RAM_ASSERT(OamShadow, 0x200);
RAM_ASSERT(SpriteTypeIndex, 0x500);
RAM_ASSERT(SpriteState, 0x520);
RAM_ASSERT(SpriteGraphicsAttributes, 0x540);
RAM_ASSERT(SpriteVertScreenPosition, 0x560);
RAM_ASSERT(SpriteVertScreenPositionSubPixel, 0x580);
RAM_ASSERT(SpriteHorizScreenPosition, 0x5A0);
RAM_ASSERT(SpriteHorizScreenPositionSubPixel, 0x5C0);
RAM_ASSERT(SpriteVertSpeedUB, 0x5E0);
RAM_ASSERT(SpriteVertSpeedLB, 0x600);
RAM_ASSERT(SpriteHorizSpeedUB, 0x620);
RAM_ASSERT(SpriteHorizSpeedLB, 0x640);
RAM_ASSERT(SpriteData1, 0x660);
RAM_ASSERT(SpriteData2, 0x670);
RAM_ASSERT(SpriteData3, 0x680);
RAM_ASSERT(SpriteData4, 0x690);
RAM_ASSERT(SpriteData5, 0x6A0);
RAM_ASSERT(SpriteData6, 0x6B0);
RAM_ASSERT(SpriteWhatDirectionToShoot, 0x6C0);
RAM_ASSERT(SpriteData8, 0x6D0);
RAM_ASSERT(SpriteWhichJeeptoAttack, 0x6E0);
RAM_ASSERT(SpriteAbsoluteHorizPositionUB, 0x6F0);
RAM_ASSERT(SpriteAbsoluteVertPositionUB, 0x700);
RAM_ASSERT(SpriteAbsoluteHorizPositionLB, 0x710);
RAM_ASSERT(SpriteObjectID, 0x720);
RAM_ASSERT(SpriteHitboxShapeIndex, 0x730);
RAM_ASSERT(SpriteHealthHP, 0x740);
RAM_ASSERT(JeepBulletDisplayFrames, 0x750);
RAM_ASSERT(JeepMainWeaponTimer, 0x75C);
RAM_ASSERT(SoundUpdateInProgress, 0x76C);
RAM_ASSERT(Level6BossTankScroll_Next, 0x76D);
RAM_ASSERT(Level6BossTankScroll_Current, 0x76E);
RAM_ASSERT(PPUUpdateQueue, 0x770);
RAM_ASSERT(HighScore, 0x7E0);
RAM_ASSERT(Jeep1Score, 0x7E4);
RAM_ASSERT(Jeep2Score, 0x7E8);
RAM_ASSERT_SIZE(0x800);

extern JACKAL_RAM JackalRam;

/* 全零 + CurrentBank=0（RESET 语义，Bank7.ASM:47 起） */
void JackalRamInit(void);

/* Label844（Bank7.ASM:1387-1409）：清 $23-$DF 与 $0300-$06FF，再 InitPPU */
void Label844(void);

#endif
