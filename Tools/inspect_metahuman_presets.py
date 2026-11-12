# What Epic's preset MetaHumans bring with them (2026-11-12: the photos showed two hairs on every head and beards on
# "women": the presets carry their own hair and beards, and the list of which are women was a guess): for each preset,
# the wardrobe items it wears, anything that says its sex, and the body constraints' names -> Saved/MetaHumanPresets.json
#   UnrealEditor-Cmd.exe AirportDynastyPro.uproject -ExecutePythonScript="<abs>/Tools/inspect_metahuman_presets.py" -unattended -RenderOffscreen
import json, os, unreal
eal = unreal.EditorAssetLibrary
mhs = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
out = {'presets': {}, 'constraints': None}
def log(m): unreal.log('ADP_MHI: ' + m)
try:
    for p in eal.list_assets('/MetaHumanCharacter/Optional/Presets', recursive=True, include_folder=False):
        ch = unreal.load_asset(p)
        if not isinstance(ch, unreal.MetaHumanCharacter): continue
        info = {'items': [], 'props': {}}
        try:
            col = ch.internal_collection
            for it in col.get_editor_property('items'):
                try:
                    wi = it.get_editor_property('wardrobe_item')
                    info['items'].append({'slot': str(it.get_editor_property('slot_name')), 'item': wi.get_path_name() if wi else str(it)})
                except Exception as e:
                    info['items'].append({'raw': str(it)[:200], 'err': str(e)[:100]})
        except Exception as e:
            info['items_err'] = str(e)[:200]
        for name in dir(ch):
            n = name.lower()
            if any(k in n for k in ('gender', 'sex', 'female', 'body_type', 'bodytype', 'age')):
                try: info['props'][name] = str(getattr(ch, name))[:120]
                except Exception as e: info['props'][name] = 'err ' + str(e)[:60]
        if out['constraints'] is None:
            try:
                if mhs.try_add_object_to_edit(ch):
                    cons = mhs.get_body_constraints(ch)
                    out['constraints'] = [{'name': str(c.name), 'active': bool(c.is_active), 'target': float(c.target_measurement), 'min': float(getattr(c, 'min_measurement', -1)), 'max': float(getattr(c, 'max_measurement', -1))} for c in cons]
                    mhs.remove_object_to_edit(ch)
            except Exception as e:
                out['constraints'] = 'err ' + str(e)[:200]
        out['presets'][ch.get_name()] = info
        log('%s: %d items %s' % (ch.get_name(), len(info['items']), info['props']))
    with open(os.path.join(unreal.Paths.project_saved_dir(), 'MetaHumanPresets.json'), 'w') as f: json.dump(out, f, indent=1)
    log('DONE')
except Exception as e:
    unreal.log_error('ADP_MHI: ' + str(e))
unreal.SystemLibrary.quit_editor()
