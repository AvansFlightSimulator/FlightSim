FUNCTION BuildDiagnosticFeedback : STRING(4095)
VAR
    output : STRING(4095);
    fragment : STRING(255);
    field : INT;
    i : INT;
    valid : BOOL;
    ok : BOOL := TRUE;
END_VAR
(* IMPLEMENTATION *)
output := '{"currentPositions":[';
ok := TRUE;
FOR i := 1 TO 6 DO
    IF i > 1 THEN ok := AppendDiagnosticText(',', output) AND ok; END_IF
    ok := AppendDiagnosticText(REAL_TO_STRING(GVL.currentPositions[i].Number), output) AND ok;
END_FOR
ok := AppendDiagnosticText('],"diagnostics":{"version":1,"sequence":', output) AND ok;
DriveDiagnostics.sampleSequence := DriveDiagnostics.sampleSequence + 1;
ok := AppendDiagnosticText(UDINT_TO_STRING(DriveDiagnostics.sampleSequence), output) AND ok;
ok := AppendDiagnosticText(',"plcTickMs":', output) AND ok;
ok := AppendDiagnosticText(UDINT_TO_STRING(DriveDiagnostics.sampleTick), output) AND ok;
ok := AppendDiagnosticText(',"sdoReadErrors":', output) AND ok;
ok := AppendDiagnosticText(UDINT_TO_STRING(DriveDiagnostics.readErrorCount), output) AND ok;
ok := AppendDiagnosticText(',"positionUnit":"Festo user unit","velocityUnit":"Festo user unit/s","registerUnit":"raw CiA402","followingErrorSource":"0x60F4 readback","targetSource":"0x607A readback"', output) AND ok;
// Field order is fixed; actuator order stays A1..A6. Null means no new valid
// sample. SDO sample ticks are kept separately from packet assembly time.
FOR field := 1 TO 11 DO
    CASE field OF
        1: fragment := ',"receivedPositions":[';
        2: fragment := ',"receivedSpeeds":[';
        3: fragment := ',"actualPositions":[';
        4: fragment := ',"actualVelocities":[';
        5: fragment := ',"driveTargetPositions":[';
        6: fragment := ',"followingErrors":[';
        7: fragment := ',"targetSampleTickMs":[';
        8: fragment := ',"followingSampleTickMs":[';
        9: fragment := ',"sdoErrorIds":[';
        10: fragment := ',"requestedPositions":[';
        11: fragment := ',"requestedSpeeds":[';
    END_CASE
    ok := AppendDiagnosticText(fragment, output) AND ok;
    FOR i := 1 TO 6 DO
        IF i > 1 THEN ok := AppendDiagnosticText(',', output) AND ok; END_IF
        valid := TRUE;
        CASE field OF
            1: fragment := REAL_TO_STRING(TcpClient.servoData.positions[i].Number);
            2: fragment := REAL_TO_STRING(TcpClient.servoData.speeds[i].Number);
            3: fragment := REAL_TO_STRING(DriveDiagnostics.actualPositions[i]); valid := DriveDiagnostics.positionValid[i];
            4: fragment := REAL_TO_STRING(DriveDiagnostics.actualVelocities[i]); valid := DriveDiagnostics.velocityValid[i];
            5: fragment := LREAL_TO_STRING(DriveDiagnostics.driveTargets[i]); valid := DriveDiagnostics.targetNew[i];
            6: fragment := LREAL_TO_STRING(DriveDiagnostics.followingErrors[i]); valid := DriveDiagnostics.followingNew[i];
            7: fragment := UDINT_TO_STRING(DriveDiagnostics.targetTick[i]); valid := DriveDiagnostics.targetNew[i];
            8: fragment := UDINT_TO_STRING(DriveDiagnostics.followingTick[i]); valid := DriveDiagnostics.followingNew[i];
            9: fragment := UINT_TO_STRING(WORD_TO_UINT(DriveDiagnostics.readErrors[i]));
            10: fragment := REAL_TO_STRING(GVL.servoPositions[i]);
            11: fragment := REAL_TO_STRING(GVL.servoVelocities[i]);
        END_CASE
        IF NOT valid THEN fragment := 'null'; END_IF
        ok := AppendDiagnosticText(fragment, output) AND ok;
    END_FOR
    ok := AppendDiagnosticText(']', output) AND ok;
END_FOR
ok := AppendDiagnosticText('}}$N', output) AND ok;
IF ok THEN
    BuildDiagnosticFeedback := output;
    FOR i := 1 TO 6 DO
        DriveDiagnostics.targetNew[i] := FALSE;
        DriveDiagnostics.followingNew[i] := FALSE;
    END_FOR
ELSE
    // Never send truncated JSON. TcpClient counts this failure for inspection.
    BuildDiagnosticFeedback := '';
END_IF
