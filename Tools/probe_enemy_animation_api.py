import unreal as u
a=u.load_asset('/Game/Assets/EnemyCharacters/AshSeer/Animations/A_AshSeer_Idle')
for prop in ['data_model_interface','data_model','controller']:
    try:
        value=a.get_editor_property(prop)
        u.log('ANIM_API '+prop+' '+str(value)+' '+str(type(value)))
        u.log('ANIM_METHODS '+str([x for x in dir(value) if any(y in x for y in ['bone','frame','model','key','bracket'])]))
    except Exception as ex:u.log('ANIM_API_ERROR '+str(ex))
u.log('CONTROLLER_CLASS '+str(u.AnimDataController))
