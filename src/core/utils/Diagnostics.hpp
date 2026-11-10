#pragma once

// Deliberately separate from verbose debug prints: never pass gateway payloads,
// tokens, HTTP bodies, URLs, or configuration contents to this logger.
void SetDiagnosticsEnabled(bool enabled);
void DiagnosticLog(const char* format, ...);
