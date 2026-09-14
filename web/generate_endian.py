from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
records={}
for name in ['save.h','records.h','resources.h']:
 text=re.sub(r'/\*.*?\*/','',(root/'include'/name).read_text(),flags=re.S)
 for body,typ in re.findall(r'typedef\s+struct\s*\{([^{}]+)\}\s*(\w+)\s*;',text):records[typ]=body
names=set(records)|{'ItemPrefix'}
lines=['/* Storage boundary: H8 numerical field values, unchanged retail EEPROM bytes. */']
lines += [f'static void Map{t}(u32 base);' for t in names]
lines+=['static void MapItemPrefix(u32 base){mapWord(base,4);mapWord(base+4,2);}']
for t,body in records.items():
 lines.append(f'static void Map{t}(u32 base){{')
 for ft,field,array in re.findall(r'(?:volatile\s+)?(\w+)\s+(\w+)\s*(\[[^\]]+\])?\s*;',body):
  if ft not in names|{'u16','s16','u32','s32'}:continue
  size=2 if ft in {'u16','s16'} else 4
  expr=f'base+offsetof({t},{field})'
  if array:
   lines.append(f'for(u32 i=0;i<sizeof((({t}*)0)->{field})/sizeof((({t}*)0)->{field}[0]);i++){{')
   expr+=f'+i*sizeof((({t}*)0)->{field}[0])'
  lines.append((f'Map{ft}({expr});' if ft in names else f'mapWord({expr},{size});'))
  if array:lines.append('}')
 lines.append('}')
lines.append('''static void MapEepromRecords(void){
 MapSaveData(EEPROM_SAVE_PRIMARY);MapSaveData(EEPROM_SAVE_BACKUP);
 MapDeviceStatus(EEPROM_STATUS_PRIMARY);MapDeviceStatus(EEPROM_STATUS_BACKUP);
 MapCourseResources(EEPROM_COURSE);MapWalkData(EEPROM_WALK);
 MapBonusResources(EEPROM_BONUS_COURSE);MapEventPokemon(EEPROM_EVENT_POKEMON);MapEventItem(EEPROM_EVENT_ITEM);
 MapPeerRecords(EEPROM_OWN_RECORDS);for(u32 i=0;i<11;i++)MapPeerRecords(EEPROM_PEER_RECORDS+i*sizeof(PeerRecords));
 MapPeerInfo(EEPROM_PEER_IMAGE+384+320);
 for(u32 i=0;i<16;i++)mapWord(EEPROM_SOUND_DIRECTORY+i*4,2);
 mapWord(EEPROM_BATTERY_PRIMARY,2);mapWord(EEPROM_BATTERY_BACKUP,2);
}''')
(root/'web/generated/endian_map.h').write_text('\n'.join(lines)+'\n')
