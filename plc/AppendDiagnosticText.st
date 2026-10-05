FUNCTION AppendDiagnosticText : BOOL
VAR_INPUT
    fragment : STRING(255);
END_VAR
VAR_IN_OUT
    destination : STRING(4095);
END_VAR
VAR
    used : INT;
    count : INT;
    i : INT;
    outputBytes : POINTER TO BYTE;
    inputBytes : POINTER TO BYTE;
END_VAR
(* IMPLEMENTATION *)
// Standard CONCAT may return STRING(255). Explicit bounded copying preserves
// larger feedback objects and always leaves a terminating NUL in PLC memory.
used := LEN(destination);
count := LEN(fragment);
AppendDiagnosticText := used + count <= 4095;
IF NOT AppendDiagnosticText THEN RETURN; END_IF
outputBytes := ADR(destination);
inputBytes := ADR(fragment);
FOR i := 0 TO count - 1 DO outputBytes[used + i] := inputBytes[i]; END_FOR
outputBytes[used + count] := 0;
