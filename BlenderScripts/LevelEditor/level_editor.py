import bpy

bl_info = {
    "name": "Level Editor",
    "author": "Ion Suzuki",
    "version": (1, 0),
    "blender": (5, 1, 0),
    "location": "",
    "desctiption": "Level Editor",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Object"
}

# addon enable callback
def register():
    print("Level Editor is enabled.")

# addon disable callback
def unregister():
    print("Level Editor is disabled.")
    
# Test running code
if __name__ == "__main__":
    register()