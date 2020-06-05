#pragma once
#include "imgui.h"

class console {

public:
	console();
	~console();

	// Portable helpers
	static int   Stricmp(const char* str1, const char* str2);
	static int   Strnicmp(const char* str1, const char* str2, int n);
	static char* Strdup(const char *str);
	static void  Strtrim(char* str);

	void    ClearLog();
	void    DeleteOldestLog();	
	void    AddLog(const char* fmt, ...) IM_FMTARGS(2);
	void    DrawAsChild(const char* title, bool* p_open);
	void    ExecCommand(const char* command_line);
	static int TextEditCallbackStub(ImGuiInputTextCallbackData* data);	// In C++11 you are better off using lambdas for this sort of forwarding callbacks
	int     TextEditCallback(ImGuiInputTextCallbackData* data);

	char                  InputBuf[256];
	ImVector<char*>       Items;
	ImVector<const char*> Commands;
	ImVector<char*>       History;
	int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
	ImGuiTextFilter       Filter;
	bool                  AutoScroll;
	bool                  ScrollToBottom;
};



