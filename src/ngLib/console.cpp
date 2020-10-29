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
		if ( log.severity == LogSeverity::LOG_DEBUG ) {
			ImGui::TextColored( ImVec4( 0.0f, 255.0f, 0.0f, 255.0f ), "%s", log.text.c_str() );
		} else if ( log.severity == LogSeverity::LOG_ERROR ) {
			ImGui::TextColored( ImVec4( 255.0f, 0.0f, 0.0f, 255.0f ), "%s", log.text.c_str() );
		} else if ( log.severity == LogSeverity::LOG_INFO ) {
			ImGui::TextColored( ImVec4( 253.0f, 213.0f, 0.0f, 255.0f ), "%s", log.text.c_str() );
		} else {
			ImGui::Text( "%s", log.text.c_str() );
		}
	}
	if ( shouldScrollDown ) {
		ImGui::SetScrollHereY( 1.0f ); // 0.0f:top, 0.5f:center, 1.0f:bottom
		shouldScrollDown = false;
	}

	ImGui::EndChild();
	if ( ImGui::Button( "Clear" ) ) {
		logs.clear();
	}

	ImGui::EndGroup();
	ImGui::End();
	mutex.unlock();
}

void Console::PrintLog( const char * text, LogSeverity severity ) {
	mutex.lock();
	Log newline{ text, severity, SysGetTimeInMicro() };
	logs.push_back( newline );
	shouldScrollDown = true;
	mutex.unlock();
}

ScopedChronoUI::~ScopedChronoUI() {
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast< std::chrono::milliseconds >( end - start ).count();
	if ( duration > 5 ) {
		ImGui::Text( "%s: %lldms\n", label, duration );
	} else {
		ImGui::Text( "%s: %lldus\n", label,
		             std::chrono::duration_cast< std::chrono::microseconds >( end - start ).count() );
	}
}

}; // namespace ng
