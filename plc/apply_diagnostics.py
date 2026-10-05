"""Run offline using CODESYS --noUI --runscript and --scriptargs 'input.project output.project'.
Creates a separate project, then builds and exports it. Never logs in or downloads.
"""
from __future__ import print_function
import io
import os
import sys
from scriptengine import *

source = os.path.dirname(os.path.abspath(__file__))
if len(sys.argv) != 3:
    raise ValueError('Expected input.project and a NEW output.project path')
if os.path.exists(sys.argv[2]):
    raise ValueError('Output exists; choose a new path to preserve previous projects')
if not os.path.isfile(sys.argv[1]):
    raise ValueError('Input project not found: ' + repr(sys.argv))
print('Opening ' + repr(sys.argv[1]))
project = projects.open(sys.argv[1])
app = project.active_application

for name, kind in [('DriveDiagnostics', PouType.Program),
                   ('AppendDiagnosticText', PouType.Function),
                   ('BuildDiagnosticFeedback', PouType.Function)]:
    with io.open(os.path.join(source, name + '.st'), encoding='utf-8') as stream:
        declaration, implementation = stream.read().split('(* IMPLEMENTATION *)')
    pou = app.create_pou(name, kind, ImplementationLanguages.st,
        return_type=('BOOL' if kind == PouType.Function else None))
    pou.textual_declaration.replace(declaration.strip())
    pou.textual_implementation.replace(implementation.strip())

tcp = [o for o in app.get_children(True) if o.get_name() == 'TcpClient' and o.has_textual_declaration][0]
declaration = tcp.textual_declaration.text
declaration = declaration.replace('sent          : STRING(gvlSetting.gc_wMaxTelegram);', 'sent          : STRING(4095);')
if 'sent          : STRING(4095);' not in declaration:
    raise ValueError('Unexpected TcpClient declaration; review before patching')
declaration = declaration.replace('END_VAR', '''    diagnosticSendTimer : TON;
    diagnosticBuildErrors : UDINT;
END_VAR''', 1)
tcp.textual_declaration.replace(declaration)
implementation = tcp.textual_implementation.text
# Keep original receive/parser and all motion logic. Replace only the active
# feedback writer, after the commented-out historical implementation.
start = implementation.index('IF fbTcpClient.xActive AND GVL.FlightSimulator.isInitialised() THEN',
    implementation.index('// IF fbTcpClient.xDone AND xConnect THEN'))
write_end = implementation.index('    // -------------------------',
    implementation.index('    fbTcpWrite(', start) + 1)
writer = '''IF fbTcpClient.xActive AND GVL.FlightSimulator.isInitialised() THEN
    DriveDiagnostics();
    diagnosticSendTimer(IN := NOT xWrite, PT := T#50MS);
    IF fbTcpWrite.xDone OR fbTcpWrite.xError THEN
        xWrite := FALSE;
    ELSIF NOT fbTcpWrite.xBusy AND diagnosticSendTimer.Q THEN
        sent := BuildDiagnosticFeedback();
        xWrite := LEN(sent) > 0;
        IF NOT xWrite THEN diagnosticBuildErrors := diagnosticBuildErrors + 1; END_IF
    END_IF
    // Buffer remains unchanged while TCP_Write is busy. Send only meaningful
    // bytes (including the newline), never the unused NUL-filled capacity.
    fbTcpWrite(xExecute := xWrite, hConnection := fbTcpClient.hConnection,
        udiTimeOut := udiTimeOut, szSize := INT_TO_UDINT(LEN(sent)), pData := ADR(sent));

'''
tcp.textual_implementation.replace(implementation[:start] + writer + implementation[write_end:])
project.save_as(sys.argv[2])
project.export_native([app], sys.argv[2] + '.export', recursive=True)
app.build()
with io.open(sys.argv[2] + '.build.txt', 'w', encoding='utf-8') as output:
    for category in system.get_message_categories():
        for message in system.get_messages(category):
            output.write(unicode(message) + u'\n')
project.close()
