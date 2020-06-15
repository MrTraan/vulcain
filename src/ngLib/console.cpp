#include "logs.h"
#include "sys.h"
#include <imgui/imgui.h>

namespace ng {

Console & GetConsole() {
	static Console instance;
	return instance;
}

static bool shouldScrollDown = false;

void Console::Draw() {
	mutex.lock();
	ImGui::Begin( "Console" );
	ImGui::BeginGroup();
	ImGui::BeginChild( ImGui::GetID( "CONSOLE ID" ) );

	for ( auto const & log : logs ) {
		if ( log.severity == LogSeverity::LOG_ERROR ) {
			ImGui::TextColored( ImVec4( 255.0f, 0.0f, 0.0f, 255.0f ), "%s", log.text.c_str() );
		} else {
			ImGui::Text( "%s", log.text.c_str() );
		}
	}
	if ( shouldScrollDown ) {
		ImGui::SetScrollHereY( 1.0f ); // 0.0f:top, 0.5f:center, 1.0f:bottom
		shouldScrollDown = false;
	}

	ImGui::EndChild();
	ImGui::EndGroup();
	ImGui::End();
	mutex.unlock();
}

void Console::PrintLog( const char * text, LogSeverity severity ) {
	mutex.lock();
	Log newline{text, severity, SysGetTimeInMicro()};
	logs.push_back( newline );
	shouldScrollDown = true;
	mutex.unlock();
}

}; // namespace ng
