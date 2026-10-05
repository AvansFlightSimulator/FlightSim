"""Prepare the two existing task objects for the CSP integration (CPython).
Usage: python prepare_csp_devices.py current.export tasks.export
Input must be a fresh native export of the original 2.1.0 project.
Device profiles and stored parameter values are changed by apply_csp.py.
"""
import sys
import xml.etree.ElementTree as E

tree = E.parse(sys.argv[1])
kept = 0
for entries in tree.getroot().findall('.//List2[@Name="EntryList"]'):
    for entry in list(entries):
        name = entry.find('./Single[@Name="MetaObject"]/Single[@Name="Name"]').text
        if name not in ['EtherCAT_Task', 'Task']:
            entries.remove(entry)
            continue
        kept += 1
        entry.find('./Single[@Name="IsRoot"]').text = 'True'
        obj = entry.find('./Single[@Name="Object"]')
        obj.find('./Single[@Name="Core"]').text = '0'
        obj.find('./Single[@Name="Core2"]').text = '0'
        if name == 'EtherCAT_Task':
            obj.find('./Single[@Name="Interval"]/Single[@Name="Time"]').text = '4'
            calls = obj.find('./List[@Name="PouList"]')
            assert len(calls) == 0, 'EtherCAT task changed; inspect before patching'
            for program in ['CSP_Cycle', 'CSP_WriteOutputs']:
                call = E.SubElement(calls, 'Single', {
                    'Type': '{f194d1ef-7376-42ce-a729-4a5485a97a46}', 'Method': 'IArchivable'})
                E.SubElement(call, 'Single', {'Name': 'Name', 'Type': 'string'}).text = program
                E.SubElement(call, 'Single', {'Name': 'Comment', 'Type': 'string'})
        else:
            obj.find('./Single[@Name="Priority"]').text = '5'
assert kept == 2, 'Expected precisely the two existing tasks'
tree.write(sys.argv[2], encoding='utf-8', xml_declaration=True)
