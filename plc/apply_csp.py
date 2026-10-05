"""Offline CODESYS integration for the existing 2.1.0 project; never downloads.
Run with input.project, output.project, source-directory, device-export arguments.
The native export changes only the existing task configuration.
"""
from __future__ import print_function
import io
import os
import sys
from scriptengine import *
from System import Guid

class ReplaceHandler(NativeImportHandler):
    def conflict(self, name, existingObject, newguid):
        return NativeImportResolve.replace
    def progress(self, name, pastedObject, exception):
        if exception: raise exception
    def skipped(self, name):
        print('Skipped', name)

def leaves(element, path=()):
    if element.is_union:
        return
    if element.has_sub_elements:
        for index, child in enumerate(element):
            for result in leaves(child, path+(index,)):
                yield result
    else:
        yield path, element

p = projects.open(sys.argv[1])
app = p.active_application
source = sys.argv[3]
objects = {}
for filename in sorted(os.listdir(source)):
    if not filename.endswith('.st') or '.' in filename[:-3]: continue
    name = filename[:-3]
    with io.open(os.path.join(source, filename), encoding='utf-8') as f: text = f.read()
    parts = text.split('(* IMPLEMENTATION *)')
    if name in ['CSP_Config', 'CSP_Data']: obj = app.create_gvl(name)
    elif name == 'CSP_AxisData': obj = app.create_dut(name)
    else:
        kind = PouType.FunctionBlock if text.startswith('FUNCTION_BLOCK') else PouType.Program if text.startswith('PROGRAM') else PouType.Function
        ret = parts[0].split('\n')[0].split(':',1)[1].strip() if kind == PouType.Function else None
        obj = app.create_pou(name, kind, ImplementationLanguages.st, return_type=ret)
    obj.textual_declaration.replace(parts[0].strip())
    if len(parts) > 1: obj.textual_implementation.replace(parts[1].strip())
    objects[name] = obj
for filename in sorted(os.listdir(source)):
    if not filename.endswith('.st') or '.' not in filename[:-3]: continue
    parent, name = filename[:-3].split('.')
    obj = objects[parent].create_method(name, language=ImplementationLanguages.st)
    with io.open(os.path.join(source,filename),encoding='utf-8') as f: decl, impl = f.read().split('(* IMPLEMENTATION *)')
    obj.textual_declaration.replace(decl.strip()); obj.textual_implementation.replace(impl.strip())

main = [o for o in app.get_children(True) if o.get_name() == 'main' and o.has_textual_declaration][0]
main.textual_declaration.replace(main.textual_declaration.text.replace('END_VAR', '    cspStartEdge : R_TRIG;\nEND_VAR', 1))
main.textual_implementation.replace('''// CSP Reset parks all six axes; a later Start enables PC targets.
cspStartEdge(CLK:=GVL.startButton OR GVL.startHMI);
IF CSP_Config.ControlMode = CSP_Config.CONTROL_MODE_CSP THEN
    IF NOT GVL.safetySystem OR CSP_Data.Fault OR NOT CSP_Data.ReadyToStart
        OR GVL.resetButton OR GVL.resetHMI OR GVL.stopButton OR GVL.stopHMI THEN bEnabled := FALSE;
    ELSIF cspStartEdge.Q THEN bEnabled := TRUE; END_IF
    CSP_Data.Enable := bEnabled AND GVL.safetySystem;
    CSP_Data.Reset := GVL.resetButton OR GVL.resetHMI;
    CSP_Data.StopRequested := GVL.stopButton OR GVL.stopHMI;
    GVL.lightRed := NOT GVL.safetySystem OR CSP_Data.Fault;
    GVL.lightGreen := NOT GVL.lightRed AND CSP_Data.ReadyToStart AND NOT CSP_Data.ResetRequired;
    GVL.lightYellow := NOT GVL.lightRed AND NOT GVL.lightGreen;
    FOR iCounter := 1 TO 6 DO
        GVL.currentPositions[iCounter].Number := LREAL_TO_REAL(CSP_Data.Axis[iCounter].ActualPosition);
        GVL.servoPositionsCurrent[iCounter] := LREAL_TO_REAL(CSP_Data.Axis[iCounter].ActualPosition);
    END_FOR
    RETURN;
END_IF
''' + main.textual_implementation.text)

tcp = [o for o in app.get_children(True) if o.get_name() == 'TcpClient' and o.has_textual_declaration][0]
tcp.textual_declaration.replace(tcp.textual_declaration.text.replace('END_VAR','    cspReceiver : CSP_Receive;\nEND_VAR',1))
impl = tcp.textual_implementation.text
# Patch only the live receive section; retain the PP implementation verbatim.
start = impl.index('    // -------------------------', impl.index('    DriveDiagnostics();'))
end = impl.index('\nEND_IF', start)
impl = impl[:start] + '    IF CSP_Config.ControlMode <> CSP_Config.CONTROL_MODE_CSP THEN\n' + impl[start:end] + '\n    END_IF\n' + impl[end:]
impl = impl.replace('    DriveDiagnostics();', '    IF CSP_Config.ControlMode <> CSP_Config.CONTROL_MODE_CSP THEN DriveDiagnostics(); END_IF')
impl = impl.replace('sent := BuildDiagnosticFeedback();', 'IF CSP_Config.ControlMode = CSP_Config.CONTROL_MODE_CSP THEN sent := CSP_Feedback();\n        ELSE sent := BuildDiagnosticFeedback(); END_IF')
impl = impl.replace('    parseJSON(\n', '''    // A CSP endpoint must never be interpreted as a legacy PP step.
    IF FIND(received, '"controlMode"') > 0 THEN xParse := FALSE; received := ''; END_IF
    parseJSON(
''')
impl += '''
IF CSP_Config.ControlMode = CSP_Config.CONTROL_MODE_CSP THEN
    cspReceiver(Connected:=fbTcpClient.xActive AND xConnect,Connection:=fbTcpClient.hConnection);
    IF cspReceiver.Error THEN xConnect := FALSE; END_IF
END_IF
'''
tcp.textual_implementation.replace(impl)
tasks = [o for o in app.get_children(True) if o.get_name().lower() == 'task configuration'][0]
tasks.import_native(sys.argv[4],
    filter=lambda name,guid,type,path: name in ['EtherCAT_Task','Task'], handler=ReplaceHandler())
# A separate project-specific profile is essential: the original installed
# device description regenerates its implicit PP output callback on import.
# Keep the vendor profile intact; the new profile moves only that final writer
# into CSP_WriteOutputs, where PP still invokes the exact vendor method.
device_repository.import_device(path=os.path.join(source,'device','FlightSim-CMMT-AS.devdesc.xml'),
    source=device_repository.sources[0], converter_factory_guid=Guid('C633F245-876F-45E8-AAB4-3FBD994C08B8'))
for device in p.get_children(True):
    if device.is_device and device.get_name().startswith('servo_drive_'):
        values = {}
        for connector in device.connectors:
            for parameter in connector.host_parameters:
                for path, element in leaves(parameter):
                    values[(connector.connector_id, parameter.id, path)] = element.value
        device.update(65, '1D_007B1A9500000009_FLIGHTSIM_CSP', '1.0.0.0')
        # Changing the engineering profile must not reset station addresses or
        # parameter values. The hardware and parameter IDs are unchanged.
        for connector in device.connectors:
            for parameter in connector.host_parameters:
                for path, element in leaves(parameter):
                    key = (connector.connector_id, parameter.id, path)
                    if key in values and element.value != values[key]:
                        element.value = values[key]
# Finish profile regeneration before setting DC periods. Setting them in the
# same profile-update session can leave the slaves at the old 8 ms on save.
p.save_as(sys.argv[2])
p.close()
p = projects.open(sys.argv[2])
# Apply live parameter values via the device API (native XML also includes
# descriptor defaults, which are not necessarily the stored user values).
for device in p.get_children(True):
    if not device.is_device: continue
    if device.get_name() == 'EtherCAT_Master' or device.get_name().startswith('servo_drive_'):
        for connector in device.connectors:
            for parameter in connector.host_parameters:
                if parameter.id in [805326848, 1610633216, 1610764288]:
                    parameter.value = '4000'
p.save()
app = p.active_application
config = [o for o in app.get_children(True) if o.get_name() == 'CSP_Config'][0]
config_text = config.textual_declaration.text
for mode in [8, 1]:
    config.textual_declaration.replace(config_text.replace('ControlMode : SINT := 1;', 'ControlMode : SINT := %d;' % mode))
    app.build()
    with io.open(sys.argv[2]+'.mode%d.build.txt' % mode,'w',encoding='utf-8') as f:
        for c in system.get_message_categories():
            for message in system.get_messages(c): f.write(unicode(message)+'\n')
config.textual_declaration.replace(config_text)
p.export_native(p.get_children(), sys.argv[2]+'.export',recursive=True)
p.save()
p.close()
