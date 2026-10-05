PROGRAM DriveDiagnostics
VAR_INPUT
    // The packet builder consumes these flags only after assembling a complete
    // message. Inputs permit that explicit handoff between CODESYS programs.
    targetNew : ARRAY[1..6] OF BOOL;
    followingNew : ARRAY[1..6] OF BOOL;
    sampleSequence : UDINT;
END_VAR
VAR
    // Passive readers only. No command, enable, reset, or axis configuration writes.
    positionReaders : ARRAY[1..6] OF MC_ReadActualPosition_Festo;
    velocityReaders : ARRAY[1..6] OF MC_ReadActualVelocity_Festo;
    parameterReader : MC_ReadParameter_Festo;
    actualPositions : ARRAY[1..6] OF REAL;
    actualVelocities : ARRAY[1..6] OF REAL;
    positionValid : ARRAY[1..6] OF BOOL;
    velocityValid : ARRAY[1..6] OF BOOL;
    driveTargets : ARRAY[1..6] OF LREAL;
    followingErrors : ARRAY[1..6] OF LREAL;
    targetTick : ARRAY[1..6] OF UDINT;
    followingTick : ARRAY[1..6] OF UDINT;
    readErrors : ARRAY[1..6] OF WORD;
    readErrorCount : UDINT;
    sampleTick : UDINT;
    pollAxis : INT := 1;
    pollFollowing : BOOL;
    pollActive : BOOL;
    pollReset : BOOL;
    pollInterval : TON;
    pollTimeout : TON;
    parameter : UINT;
    drive : REFERENCE TO AXIS_REF_FESTO;
    i : INT;
END_VAR
(* IMPLEMENTATION *)
// User units are supplied by the Festo library. Valid AND NOT Error is required;
// never turn a failed read into a plausible zero-valued measurement.
positionReaders[1](Axis := servo_drive_1, Enable := TRUE);
positionReaders[2](Axis := servo_drive_2, Enable := TRUE);
positionReaders[3](Axis := servo_drive_3, Enable := TRUE);
positionReaders[4](Axis := servo_drive_4, Enable := TRUE);
positionReaders[5](Axis := servo_drive_5, Enable := TRUE);
positionReaders[6](Axis := servo_drive_6, Enable := TRUE);
velocityReaders[1](Axis := servo_drive_1, Enable := TRUE);
velocityReaders[2](Axis := servo_drive_2, Enable := TRUE);
velocityReaders[3](Axis := servo_drive_3, Enable := TRUE);
velocityReaders[4](Axis := servo_drive_4, Enable := TRUE);
velocityReaders[5](Axis := servo_drive_5, Enable := TRUE);
velocityReaders[6](Axis := servo_drive_6, Enable := TRUE);
sampleTick := TIME_TO_UDINT(TIME());
FOR i := 1 TO 6 DO
    positionValid[i] := positionReaders[i].Valid AND NOT positionReaders[i].Error;
    velocityValid[i] := velocityReaders[i].Valid AND NOT velocityReaders[i].Error;
    actualPositions[i] := positionReaders[i].Position;
    actualVelocities[i] := velocityReaders[i].Velocity;
END_FOR

// Only one SDO request is active across all six drives. Each request is followed
// by a 50 ms idle interval. Register traces are slower than the 20 Hz PDO traces.
// Do not increase this rate without measuring the actual fieldbus load.
CASE pollAxis OF
    1: drive REF= servo_drive_1;
    2: drive REF= servo_drive_2;
    3: drive REF= servo_drive_3;
    4: drive REF= servo_drive_4;
    5: drive REF= servo_drive_5;
    6: drive REF= servo_drive_6;
END_CASE
IF pollFollowing THEN parameter := 16#60F4; ELSE parameter := 16#607A; END_IF
IF pollReset THEN
    parameterReader(Axis := drive, Enable := FALSE);
    pollTimeout(IN := FALSE);
    pollInterval(IN := FALSE);
    pollReset := FALSE;
    IF pollFollowing THEN
        pollAxis := pollAxis + 1;
        IF pollAxis > 6 THEN pollAxis := 1; END_IF
    END_IF
    pollFollowing := NOT pollFollowing;
ELSE
    pollInterval(IN := NOT pollActive, PT := T#50MS);
    IF pollInterval.Q THEN pollActive := TRUE; END_IF
    parameterReader(Axis := drive, Enable := pollActive,
        ParameterNumber := parameter, SubindexNumber := 0);
    pollTimeout(IN := pollActive, PT := T#1S);
    IF pollActive AND (parameterReader.Valid OR parameterReader.Error OR pollTimeout.Q) THEN
        IF parameterReader.Valid AND NOT parameterReader.Error THEN
            IF pollFollowing THEN
                followingErrors[pollAxis] := parameterReader.Value;
                followingTick[pollAxis] := sampleTick;
                followingNew[pollAxis] := TRUE;
            ELSE
                driveTargets[pollAxis] := parameterReader.Value;
                targetTick[pollAxis] := sampleTick;
                targetNew[pollAxis] := TRUE;
            END_IF
            readErrors[pollAxis] := 0;
        ELSE
            readErrorCount := readErrorCount + 1;
            IF pollTimeout.Q THEN readErrors[pollAxis] := 16#FFFF;
            ELSE readErrors[pollAxis] := parameterReader.ErrorID; END_IF
            IF pollFollowing THEN followingNew[pollAxis] := FALSE;
            ELSE targetNew[pollAxis] := FALSE; END_IF
        END_IF
        pollActive := FALSE;
        pollReset := TRUE;
    END_IF
END_IF
