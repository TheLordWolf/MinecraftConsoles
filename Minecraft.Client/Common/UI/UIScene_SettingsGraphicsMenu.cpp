#include "stdafx.h"
#include "UI.h"
#include "UIScene_SettingsGraphicsMenu.h"
#include "..\..\Minecraft.h"
#include "..\..\Options.h"
#include "..\..\GameRenderer.h"

#include "../../LevelRenderer.h"

namespace
{
    constexpr int FOV_MIN = 70;
    constexpr int FOV_MAX = 110;
    constexpr int FOV_SLIDER_MAX = 100;

	int ClampFov(int value)
	{
		if (value < FOV_MIN) return FOV_MIN;
		if (value > FOV_MAX) return FOV_MAX;
		return value;
	}

    [[maybe_unused]]
    int FovToSliderValue(float fov)
	{
		const int clampedFov = ClampFov(static_cast<int>(fov + 0.5f));
		return ((clampedFov - FOV_MIN) * FOV_SLIDER_MAX) / (FOV_MAX - FOV_MIN);
	}

	int sliderValueToFov(int sliderValue)
	{
		if (sliderValue < 0) sliderValue = 0;
		if (sliderValue > FOV_SLIDER_MAX) sliderValue = FOV_SLIDER_MAX;
		return FOV_MIN + ((sliderValue * (FOV_MAX - FOV_MIN)) / FOV_SLIDER_MAX);
	}
}

int UIScene_SettingsGraphicsMenu::LevelToDistance(int level)
{
	static const int table[6] = {2,4,8,16,32,64};
	if(level < 0) level = 0;
	if(level > 5) level = 5;
	return table[level];
}

int UIScene_SettingsGraphicsMenu::DistanceToLevel(int dist)
{
    static const int table[6] = {2,4,8,16,32,64};
    for(int i = 0; i < 6; i++){
        if(table[i] == dist)
            return i;
    }
    return 3;
}

UIScene_SettingsGraphicsMenu::UIScene_SettingsGraphicsMenu(int iPad, void *initData, UILayer *parentLayer) : UIScene(iPad, parentLayer)
{
	// Setup all the Iggy references we need for this scene
	initialiseMovie();
	Minecraft* pMinecraft = Minecraft::GetInstance();
	
	m_bNotInGame=(Minecraft::GetInstance()->level==nullptr);

	m_checkboxClouds.init(app.GetString(IDS_CHECKBOX_RENDER_CLOUDS),eControl_Clouds,(app.GetGameSettings(m_iPad,eGameSetting_Clouds)!=0));
	m_checkboxBedrockFog.init(app.GetString(IDS_CHECKBOX_RENDER_BEDROCKFOG),eControl_BedrockFog,(app.GetGameSettings(m_iPad,eGameSetting_BedrockFog)!=0));
	m_checkboxCustomSkinAnim.init(app.GetString(IDS_CHECKBOX_CUSTOM_SKIN_ANIM),eControl_CustomSkinAnim,(app.GetGameSettings(m_iPad,eGameSetting_CustomSkinAnim)!=0));

	
	WCHAR TempString[256];

	swprintf(TempString, 256, L"Render Distance: %d",app.GetGameSettings(m_iPad,eGameSetting_RenderDistance));	
	m_sliderRenderDistance.init(TempString,eControl_RenderDistance,0,5,DistanceToLevel(app.GetGameSettings(m_iPad,eGameSetting_RenderDistance)));
	
	swprintf( TempString, 256, L"%ls: %d%%", app.GetString( IDS_SLIDER_GAMMA ),app.GetGameSettings(m_iPad,eGameSetting_Gamma));	
	m_sliderGamma.init(TempString,eControl_Gamma,0,100,app.GetGameSettings(m_iPad,eGameSetting_Gamma));

    const int initialFovSlider = app.GetGameSettings(m_iPad, eGameSetting_FOV);
	const int initialFovDeg = sliderValueToFov(initialFovSlider);
	swprintf(TempString, 256, L"FOV: %d", initialFovDeg);
	m_sliderFOV.init(TempString, eControl_FOV, 0, FOV_SLIDER_MAX, initialFovSlider);

	//chunk-memory (maybe ?) temporary checkout
	int allocatedVal = app.GetGameSettingsUInt(m_iPad, eGameSetting_ChunkCommandBufferMem);
	if (allocatedVal < LevelRenderer::MIN_COMMANDBUFFER_ALLOCATIONS)
		allocatedVal = LevelRenderer::MIN_COMMANDBUFFER_ALLOCATIONS;
	if (allocatedVal > LevelRenderer::MAX_COMMANDBUFFER_ALLOCATIONS)
		allocatedVal = LevelRenderer::MAX_COMMANDBUFFER_ALLOCATIONS;

	constexpr unsigned int MIN_COMMAND_BUFFER_MB = LevelRenderer::MIN_COMMANDBUFFER_ALLOCATIONS / 1024 / 1024;
	constexpr unsigned int MAX_COMMAND_BUFFER_MB = LevelRenderer::MAX_COMMANDBUFFER_ALLOCATIONS / 1024 / 1024;
	swprintf(TempString, 256, L"Chunk memory: %dMB", allocatedVal / 1024 / 1024);
	m_sliderChunkCommandBufferMem.init(TempString, eControl_ChunkCommandBufferMem, MIN_COMMAND_BUFFER_MB, MAX_COMMAND_BUFFER_MB, allocatedVal);

	swprintf(TempString, 256, L"Chunk Near Distance: %d", app.GetGameSettings(m_iPad, eGameSetting_ChunkNearDistance));
	//swprintf(TempString, 256, L"Chunk Near Distance: %d", 20);
	m_sliderChunkNearDistance.init(TempString, eControl_ChunkNearDistance, LevelRenderer::MIN_NEAR_DISTANCE, LevelRenderer::MAX_NEAR_DISTANCE, app.GetGameSettings(m_iPad, eGameSetting_ChunkNearDistance));

	swprintf(TempString, 256, L"Chunk Force Update : %dMS", app.GetGameSettings(m_iPad, eGameSetting_ChunkForceUpdatePeriodMS));
	m_sliderChunkForceUpdatePeriodMS.init(TempString, eControl_ChunkForceUpdatePeriodMS, LevelRenderer::MIN_FORCE_DIRTY_CHUNK_CHECK_PERIOD_MS, LevelRenderer::MAX_FORCE_DIRTY_CHUNK_CHECK_PERIOD_MS, app.GetGameSettings(m_iPad, eGameSetting_ChunkForceUpdatePeriodMS));


	swprintf( TempString, 256, L"%ls: %d%%", app.GetString( IDS_SLIDER_INTERFACEOPACITY ),app.GetGameSettings(m_iPad,eGameSetting_InterfaceOpacity));	
	m_sliderInterfaceOpacity.init(TempString,eControl_InterfaceOpacity,0,100,app.GetGameSettings(m_iPad,eGameSetting_InterfaceOpacity));


	doHorizontalResizeCheck();

	const bool bInGame=(Minecraft::GetInstance()->level!=nullptr);
	const bool bIsPrimaryPad=(ProfileManager.GetPrimaryPad()==m_iPad);
	// if we're not in the game, we need to use basescene 0 
	if(bInGame)
	{
		// If the game has started, then you need to be the host to change the in-game gamertags
		if(bIsPrimaryPad)
		{	
			// we are the primary player on this machine, but not the game host
			// are we the game host? If not, we need to remove the bedrockfog setting
			if(!g_NetworkManager.IsHost())
			{
				// hide the in-game bedrock fog setting
				removeControl(&m_checkboxBedrockFog, true);
			}
		}
		else
		{
			// We shouldn't have the bedrock fog option, or the m_CustomSkinAnim option
			removeControl(&m_checkboxBedrockFog, true);
			removeControl(&m_checkboxCustomSkinAnim, true);
		}
	}

	if(app.GetLocalPlayerCount()>1)
	{
#if TO_BE_IMPLEMENTED
		app.AdjustSplitscreenScene(m_hObj,&m_OriginalPosition,m_iPad);
#endif
	}
}

UIScene_SettingsGraphicsMenu::~UIScene_SettingsGraphicsMenu()
{
}

wstring UIScene_SettingsGraphicsMenu::getMoviePath()
{
	if(app.GetLocalPlayerCount() > 1)
	{
		return L"SettingsGraphicsMenuSplit";
	}
	else
	{
		return L"SettingsGraphicsMenu";
	}
}

void UIScene_SettingsGraphicsMenu::updateTooltips()
{
	ui.SetTooltips( m_iPad, IDS_TOOLTIPS_SELECT,IDS_TOOLTIPS_BACK);
}

void UIScene_SettingsGraphicsMenu::updateComponents()
{
	const bool bNotInGame=(Minecraft::GetInstance()->level==nullptr);
	if(bNotInGame)
	{
		m_parentLayer->showComponent(m_iPad,eUIComponent_Panorama,true);
		m_parentLayer->showComponent(m_iPad,eUIComponent_Logo,true);
	}
	else
	{
		m_parentLayer->showComponent(m_iPad,eUIComponent_Panorama,false);
	
		if( app.GetLocalPlayerCount() == 1 ) m_parentLayer->showComponent(m_iPad,eUIComponent_Logo,true);
		else m_parentLayer->showComponent(m_iPad,eUIComponent_Logo,false);

	}
}

void UIScene_SettingsGraphicsMenu::handleInput(int iPad, int key, bool repeat, bool pressed, bool released, bool &handled)
{
	ui.AnimateKeyPress(iPad, key, repeat, pressed, released);
	switch(key)
	{
	case ACTION_MENU_CANCEL:
		if(pressed)
		{
			// check the checkboxes
			app.SetGameSettings(m_iPad,eGameSetting_Clouds,m_checkboxClouds.IsChecked()?1:0);
			app.SetGameSettings(m_iPad,eGameSetting_BedrockFog,m_checkboxBedrockFog.IsChecked()?1:0);
			app.SetGameSettings(m_iPad,eGameSetting_CustomSkinAnim,m_checkboxCustomSkinAnim.IsChecked()?1:0);

			navigateBack();
			handled = true;
		}
		break;
	case ACTION_MENU_OK:
#ifdef __ORBIS__
	case ACTION_MENU_TOUCHPAD_PRESS:
#endif
		sendInputToMovie(key, repeat, pressed, released);
		break;
	case ACTION_MENU_UP:
	case ACTION_MENU_DOWN:
	case ACTION_MENU_LEFT:
	case ACTION_MENU_RIGHT:
		sendInputToMovie(key, repeat, pressed, released);
		break;
	}
}

void UIScene_SettingsGraphicsMenu::handleSliderMove(F64 sliderId, F64 currentValue)
{
	WCHAR TempString[256];
	const int value = static_cast<int>(currentValue);
	switch(static_cast<int>(sliderId))
	{
	case eControl_RenderDistance:
		{
			m_sliderRenderDistance.handleSliderMove(value);

			const int dist = LevelToDistance(value);

			app.SetGameSettings(m_iPad,eGameSetting_RenderDistance,dist);

			const Minecraft* mc = Minecraft::GetInstance();
			mc->options->viewDistance = 3 - value;
			swprintf(TempString,256,L"Render Distance: %d",dist);
			m_sliderRenderDistance.setLabel(TempString);
		}
		break;

	case eControl_Gamma:
		m_sliderGamma.handleSliderMove(value);
		
		app.SetGameSettings(m_iPad,eGameSetting_Gamma,value);
		swprintf( TempString, 256, L"%ls: %d%%", app.GetString( IDS_SLIDER_GAMMA ),value);
		m_sliderGamma.setLabel(TempString);

		break;

	case eControl_FOV:
		{
			m_sliderFOV.handleSliderMove(value);
			const Minecraft* pMinecraft = Minecraft::GetInstance();
			const int fovValue = sliderValueToFov(value);
			pMinecraft->gameRenderer->SetFovVal(static_cast<float>(fovValue));
			app.SetGameSettings(m_iPad, eGameSetting_FOV, value);
			WCHAR tempString[256];
			swprintf(tempString, 256, L"FOV: %d", fovValue);
			m_sliderFOV.setLabel(tempString);
		}
		break;
	case eControl_ChunkCommandBufferMem:
		{
			const int roundedVal = value - (value % 128);
			m_sliderChunkCommandBufferMem.handleSliderMove(roundedVal);
			app.SetGameSettingsUInt(m_iPad, eGameSetting_ChunkCommandBufferMem, static_cast<unsigned int>(roundedVal * 1024 * 1024));

			std::string str;
			str = "chunk-memory alloc UI : " + std::to_string(roundedVal) + "\n";
			app.DebugPrintf(str.c_str());
			swprintf(TempString, 256, L"Chunk memory: %dMB", roundedVal);
			m_sliderChunkCommandBufferMem.setLabel(TempString);
		}
		break;
	case eControl_ChunkNearDistance:
		{
			m_sliderChunkNearDistance.handleSliderMove(value);
			app.SetGameSettings(m_iPad, eGameSetting_ChunkNearDistance,value);

			std::string str;
			str = "chunk-near  UI : " + std::to_string(value) + "\n";
			app.DebugPrintf(str.c_str());

			swprintf(TempString, 256, L"Chunk Near Distance: %d", value);
			m_sliderChunkNearDistance.setLabel(TempString);

		}
		break;
	case eControl_ChunkForceUpdatePeriodMS:
		{
			m_sliderChunkForceUpdatePeriodMS.handleSliderMove(value);
			app.SetGameSettings(m_iPad, eGameSetting_ChunkForceUpdatePeriodMS, value);

			std::string str;
			str = "chunk-force  UI : " + std::to_string(value) + "\n";
			app.DebugPrintf(str.c_str());

			swprintf(TempString, 256, L"Chunk Force Update : %dms", value);
			m_sliderChunkForceUpdatePeriodMS.setLabel(TempString);

		}
		break;
	case eControl_InterfaceOpacity:
		m_sliderInterfaceOpacity.handleSliderMove(value);
		
		app.SetGameSettings(m_iPad,eGameSetting_InterfaceOpacity,value);
		swprintf( TempString, 256, L"%ls: %d%%", app.GetString( IDS_SLIDER_INTERFACEOPACITY ),value);	
		m_sliderInterfaceOpacity.setLabel(TempString);

		break;
	}
}
