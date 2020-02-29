#include "stdafx.h"
#include "ChatOverlay.hpp"
#include <time.h>


#include "MultiplayerScreen.hpp"


void ChatOverlay::UpdateNuklearInput(SDL_Event evt)
{
	if (!m_isOpen || m_forceToBottom)
		return;
	m_eventQueue.push(evt);
}

// TODO write clweanup function


bool ChatOverlay::Init()
{
	m_nctx = nk_sdl_init((SDL_Window*)g_gameWindow->Handle());

	g_gameWindow->OnAnyEvent.Add(this, &ChatOverlay::UpdateNuklearInput);
	{
		struct nk_font_atlas *atlas;
		nk_sdl_font_stash_begin(&atlas);
		struct nk_font *fallback = nk_font_atlas_add_from_file(atlas, Path::Normalize( Path::Absolute("fonts/settings/NotoSans-Regular.ttf")).c_str(), 24, 0);

		struct nk_font_config cfg_kr = nk_font_config(24);
		cfg_kr.merge_mode = nk_true;
		cfg_kr.range = nk_font_korean_glyph_ranges();

		NK_STORAGE const nk_rune jp_ranges[] = {
			0x0020, 0x00FF,
			0x3000, 0x303f,
			0x3040, 0x309f,
			0x30a0, 0x30ff,
			0x4e00, 0x9faf,
			0xff00, 0xffef,
			0
		};
		struct nk_font_config cfg_jp = nk_font_config(24);
		cfg_jp.merge_mode = nk_true;
		cfg_jp.range = jp_ranges;

		NK_STORAGE const nk_rune cjk_ranges[] = {
			0x0020, 0x00FF,
			0x3000, 0x30FF,
			0x3131, 0x3163,
			0xAC00, 0xD79D,
			0x31F0, 0x31FF,
			0xFF00, 0xFFEF,
			0x4e00, 0x9FAF,
			0
		};

		struct nk_font_config cfg_cjk = nk_font_config(24);
		cfg_cjk.merge_mode = nk_true;
		cfg_cjk.range = cjk_ranges;

		int maxSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
		Logf("System max texture size: %d", Logger::Info, maxSize);
		if (maxSize >= FULL_FONT_TEXTURE_HEIGHT && !g_gameConfig.GetBool(GameConfigKeys::LimitSettingsFont))
		{
			nk_font_atlas_add_from_file(atlas, Path::Normalize(Path::Absolute("fonts/settings/DroidSansFallback.ttf")).c_str(), 24, &cfg_cjk);
		}
		
		nk_sdl_font_stash_end();
		nk_font_atlas_cleanup(atlas);
		//nk_style_load_all_cursors(m_nctx, atlas->cursors);
		nk_style_set_font(m_nctx, &fallback->handle);
	}
	
	m_nctx->style.text.color = nk_rgb(255, 255, 255);
	m_nctx->style.button.border_color = nk_rgb(0, 128, 255);
	m_nctx->style.button.padding = nk_vec2(5,5);
	m_nctx->style.button.rounding = 0;
	m_nctx->style.window.fixed_background = nk_style_item_color(nk_rgb(40, 40, 40));
	m_nctx->style.slider.bar_normal = nk_rgb(20, 20, 20);
	m_nctx->style.slider.bar_hover = nk_rgb(20, 20, 20);
	m_nctx->style.slider.bar_active = nk_rgb(20, 20, 20);

	// Init the socket callbacks
	m_multi->GetTCP().SetTopicHandler("server.chat.received", this, &ChatOverlay::m_handleChatReceived);

	AddMessage("Note: This chat is currently not encrypted", 179, 73, 73); 
}

void ChatOverlay::Tick(float deltatime)
{
	nk_input_begin(m_nctx);
	while (!m_eventQueue.empty())
	{
		nk_sdl_handle_event(&m_eventQueue.front());
		m_eventQueue.pop();
	}
	nk_input_end(m_nctx);

	if (m_isOpen && nk_window_find(m_nctx, "Multiplayer Chat") && 
			nk_window_is_closed(m_nctx, "Multiplayer Chat"))
	{
		CloseChat();
	}
}

void ChatOverlay::NKRender()
{
	nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
}

bool
nk_edit_isfocused(struct nk_context *ctx)
{
    struct nk_window *win;
    if (!ctx || !ctx->current) return false;

    win = ctx->current;
	return win->edit.active;
}

void ChatOverlay::m_drawWindow()
{
	//const int windowFlag = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_CLOSABLE;
	const int windowFlag = NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE ;

	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
	float x = g_resolution.x / 2 - w / 2;


	if (!nk_begin(m_nctx, "Multiplayer Chat", nk_rect(0, g_resolution.y - 400, g_resolution.x, 400), windowFlag))
	{
		return;
	}

	nk_layout_set_min_row_height(m_nctx, 20);
	float box_height = nk_window_get_height(m_nctx) - 110;
	nk_layout_row_dynamic(m_nctx, box_height, 1);

	if (nk_group_scrolled_begin(m_nctx, &m_chatScroll, "Chatbox", NK_WINDOW_BORDER)) {

		struct nk_vec2 start_pos = nk_widget_position(m_nctx);

		nk_layout_row_dynamic(m_nctx, 30, 1);

		for(auto v : m_messages) {
			nk_label_colored(m_nctx, v.first.c_str(), NK_LEFT, v.second);
		}
		
		struct nk_vec2 end_pos = nk_widget_position(m_nctx);
		if (m_newMessage)
		{
			float end_pos_rel = end_pos.y - box_height;
			if (end_pos_rel < 250 || m_forceToBottom)
			{
				m_chatScroll.y = end_pos.y - start_pos.y;
			}
			m_newMessage = false;
		}

		nk_group_scrolled_end(m_nctx);

	}


	nk_layout_row_dynamic(m_nctx, 40, 1);

	if (m_forceToBottom)
	{
		nk_edit_focus(m_nctx, NK_EDIT_ALWAYS_INSERT_MODE);
		memset(m_chatDraft, 0, sizeof(m_chatDraft));
		m_forceToBottom = false;
	}
	m_inEdit = nk_edit_isfocused(m_nctx);

	nk_edit_string_zero_terminated(m_nctx, NK_EDIT_FIELD, m_chatDraft, sizeof(m_chatDraft)-1, nk_filter_default);


	nk_end(m_nctx);
}

void ChatOverlay::Render(float deltatime) {
	float w = Math::Min(g_resolution.y / 1.4, g_resolution.x - 5.0);
	float x = g_resolution.x / 2 - w / 2;


	if (m_isOpen)
	{
		m_drawWindow();
	}

	g_application->ForceRender();
	NKRender();
}

void ChatOverlay::CloseChat()
{
	m_isOpen = false;
	m_inEdit = false;
	m_forceToBottom = false;
}

void ChatOverlay::OpenChat()
{
	m_isOpen = true;
	m_forceToBottom = true;
}

bool ChatOverlay::OnKeyPressedConsume(int32 key)
{
	if (key == SDLK_ESCAPE && m_isOpen)
	{
		CloseChat();
		return true;
	}

	if (key == SDLK_RETURN && m_isOpen)
	{
		// Send message if there is one
		if (strlen(m_chatDraft) > 0) {
			SendChatMessage(m_chatDraft);
			memset(m_chatDraft, 0, sizeof(m_chatDraft));
		}
		return true;
	}

	if (m_inEdit)
		return true;

	if (key == SDLK_BACKQUOTE)
	{
		// Toggle open
		if (m_isOpen)
			CloseChat();
		else
			OpenChat();
		return true;
	}

	return m_isOpen;

}

void ChatOverlay::SendChatMessage(const String& message)
{
	nlohmann::json packet;
	if (m_multi->InRoom())
		packet["topic"] = "room.chat.send";
	else 
		packet["topic"] = "server.chat.send";

	packet["message"] = message;
	m_multi->GetTCP().SendJSON(packet);

    time_t t = time(NULL);
	struct tm ttm = * localtime(&t);
	String out = Utility::Sprintf("%02u:%02u [%s] %s", ttm.tm_hour, ttm.tm_min, m_multi->GetUserName(), message);


	AddMessage(out, 200, 200, 200);

}

bool ChatOverlay::m_handleChatReceived(nlohmann::json& packet)
{
	String message;
	packet["message"].get_to(message);

    time_t t = time(NULL);
	struct tm ttm = * localtime(&t);

	String out = Utility::Sprintf("%02u:%02u %s", ttm.tm_hour, ttm.tm_min, message);
	AddMessage(out);
}

void ChatOverlay::AddMessage(const String& message, int r, int g, int b)
{
	m_messages.push_back(std::make_pair(message, nk_rgb(r,g,b)));
	m_newMessage = true;
}

void ChatOverlay::AddMessage(const String& message)
{
	m_messages.push_back(std::make_pair(message, nk_rgb(256, 256, 256)));
	m_newMessage = true;
}
