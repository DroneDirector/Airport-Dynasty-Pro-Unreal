# Second look (2026-11-12): the collection's and the instance's API (what lists a preset's own hair and beard, and how to
# take them off), on two presets; and the sex of each preset as its own body says (the chest / hip measurements).
import json, os, unreal
eal = unreal.EditorAssetLibrary
mhs = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
def log(m): unreal.log('ADP_MHI2: ' + m)
out = {}
try:
    for name in ('Aoi', 'Jelena'):
        ch = unreal.load_asset('/MetaHumanCharacter/Optional/Presets/' + name)
        col = ch.internal_collection; inst = col.default_instance
        log('collection dir: ' + ' '.join(n for n in dir(col) if not n.startswith('_')))
        log('instance dir: ' + ' '.join(n for n in dir(inst) if not n.startswith('_')))
        for attr in ('items', 'slot_selections', 'pipeline'):
            for obj, oname in ((col, 'collection'), (inst, 'instance')):
                try: log('%s %s.%s = %s' % (name, oname, attr, str(obj.get_editor_property(attr))[:600]))
                except Exception as e: log('%s %s.%s: %s' % (name, oname, attr, str(e)[:80]))
        for meth in ('get_items', 'get_slot_selections', 'get_item_paths', 'get_all_items'):
            for obj, oname in ((col, 'collection'), (inst, 'instance')):
                if hasattr(obj, meth):
                    try: log('%s %s.%s() = %s' % (name, oname, meth, str(getattr(obj, meth)())[:600]))
                    except Exception as e: log('%s %s.%s(): %s' % (name, oname, meth, str(e)[:80]))
        log('subsystem dir: ' + ' '.join(n for n in dir(mhs) if not n.startswith('_') and ('item' in n.lower() or 'slot' in n.lower() or 'wardrobe' in n.lower() or 'groom' in n.lower() or 'remove' in n.lower())))
    # every preset's own measurements: women have the bust / hip, men the shoulders
    for p in eal.list_assets('/MetaHumanCharacter/Optional/Presets', recursive=True, include_folder=False):
        ch = unreal.load_asset(p)
        if not isinstance(ch, unreal.MetaHumanCharacter): continue
        if mhs.try_add_object_to_edit(ch):
            cons = {str(c.name): round(float(c.target_measurement), 1) for c in mhs.get_body_constraints(ch)}
            mhs.remove_object_to_edit(ch)
            out[ch.get_name()] = cons
            log('%s: %s' % (ch.get_name(), cons))
    with open(os.path.join(unreal.Paths.project_saved_dir(), 'MetaHumanPresetBodies.json'), 'w') as f: json.dump(out, f, indent=1)
    log('DONE')
except Exception as e:
    unreal.log_error('ADP_MHI2: ' + str(e))
unreal.SystemLibrary.quit_editor()
