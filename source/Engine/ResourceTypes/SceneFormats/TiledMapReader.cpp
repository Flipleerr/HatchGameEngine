#if INTERFACE
#include <Engine/IO/Stream.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>
class TiledMapReader {
public:
    // static bool             Initialized;
};
#endif

#include <Engine/ResourceTypes/SceneFormats/TiledMapReader.h>

#include <Engine/IO/MemoryStream.h>
#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Includes/HashMap.h>
#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene.h>

#include <Engine/TextFormats/XML/XMLParser.h>

#define TILE_FLIPX_MASK 0x80000000U
#define TILE_FLIPY_MASK 0x40000000U
#define TILE_COLLA_MASK 0x30000000U
#define TILE_COLLB_MASK 0x0C000000U
#define TILE_COLLC_MASK 0x03000000U
#define TILE_IDENT_MASK 0x00FFFFFFU

static const int decoding[] = {
    62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1,
    -1, -1, -2, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
    -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
    36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51 };
int   base64_decode_value(int8_t value_in) {
    value_in -= 43;
    if (value_in < 0 || value_in >= 80) { return -1; }
    return decoding[(int)value_in];
}
int   base64_decode_block(const char* code_in, const int length_in, char* plaintext_out) {
    const char* codechar = code_in;
    int8_t* plainchar = (int8_t*)plaintext_out;
    int8_t fragment;

    int  _step = 0;
    int8_t _plainchar = 0;

    *plainchar = _plainchar;

    switch (_step) {
        while (1) {
            case 0:
                do {
                    if (codechar >= code_in + length_in) {
                        _step = 0;
                        _plainchar = *plainchar;
                        return (int)(plainchar - (int8_t*)plaintext_out);
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
                *plainchar    = (fragment & 0x03f) << 2;

            case 1:
                do {
                    if (codechar >= code_in + length_in) {
                        _step = 1;
                        _plainchar = *plainchar;
                        return (int)(plainchar - (int8_t*)plaintext_out);
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
                *plainchar++ |= (fragment & 0x030) >> 4;
                *plainchar    = (fragment & 0x00f) << 4;

            case 2:
                do {
                    if (codechar >= code_in + length_in) {
                        _step = 2;
                        _plainchar = *plainchar;
                        return (int)(plainchar - (int8_t*)plaintext_out);
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
                *plainchar++ |= (fragment & 0x03c) >> 2;
                *plainchar    = (fragment & 0x003) << 6;

            case 3:
                do {
                    if (codechar >= code_in + length_in) {
                        _step = 3;
                        _plainchar = *plainchar;
                        return (int)(plainchar - (int8_t*)plaintext_out);
                    }
                    fragment = (char)base64_decode_value(*codechar++);
                } while (fragment < 0);
                *plainchar++   |= (fragment & 0x03f);
        }
    }
    /* control should not reach here */
    return (int)(plainchar - (int8_t*)plaintext_out);
}

PRIVATE STATIC VMValue TiledMapReader::ParseProperty(XMLNode* property) {
    // If the property has no value (for example, a multiline string),
    // the value is assumed to be in the content
    if (!property->attributes.Exists("value")) {
        // FIXME: check if this is needed
        if (property->children.size() == 0)
            return NULL_VAL;

        XMLNode* text = property->children[0];
        return OBJECT_VAL(CopyString(text->name.Start, (int)text->name.Length));
    }

    Token    property_type = property->attributes.Get("type");
    Token    property_value = property->attributes.Get("value");

    float fx, fy;
    VMValue val = NULL_VAL;
    if (XMLParser::MatchToken(property_type, "int")) {
        val = INTEGER_VAL((int)XMLParser::TokenToNumber(property_value));
    }
    else if (XMLParser::MatchToken(property_type, "float")) {
        val = DECIMAL_VAL(XMLParser::TokenToNumber(property_value));
    }
    else if (XMLParser::MatchToken(property_type, "bool")) {
        if (XMLParser::MatchToken(property_value, "false"))
            val = INTEGER_VAL(0);
        else
            val = INTEGER_VAL(1);
    }
    else if (XMLParser::MatchToken(property_type, "color")) {
        /*
        property_value:
        In "#AARRGGBB" hex format, can also be "" empty
        */
        int hexCol;
        if (property_value.Length == 0) {
            val = INTEGER_VAL(0);
        }
        else if (sscanf(property_value.Start, "#%X", &hexCol) == 1) {
            val = INTEGER_VAL(hexCol);
        }
        else {
            val = INTEGER_VAL(0);
        }
    }
    else if (XMLParser::MatchToken(property_type, "file")) {
        /*
        property_value:
        just a string
        */
        val = OBJECT_VAL(CopyString(property_value.Start, property_value.Length));
    }
    else if (XMLParser::MatchToken(property_type, "object")) {
        /*
        property_value:
        an integer
        */
        val = INTEGER_VAL((int)XMLParser::TokenToNumber(property_value));
    }
    else if (sscanf(property_value.Start, "vec2 %f,%f", &fx, &fy) == 2) {
        VMValue valX = DECIMAL_VAL(fx);
        VMValue valY = DECIMAL_VAL(fy);

        ObjArray* array = NewArray();
        array->Values->push_back(valX);
        array->Values->push_back(valY);
        val = OBJECT_VAL(array);
    }
    else { // implied as string
        val = OBJECT_VAL(CopyString(property_value.Start, property_value.Length));
    }

    return val;
}

PRIVATE STATIC void TiledMapReader::ParsePropertyNode(XMLNode* node, HashMap<VMValue>* properties) {
    if (!node->attributes.Exists("name"))
        return;

    Token    property_name = node->attributes.Get("name");

    char*  object_attribute_name = (char*)malloc(property_name.Length + 1);
    memcpy(object_attribute_name, property_name.Start, property_name.Length);
    object_attribute_name[property_name.Length] = 0;

    properties->Put(object_attribute_name, TiledMapReader::ParseProperty(node));

    free(object_attribute_name);
}

PRIVATE STATIC ObjArray* TiledMapReader::ParsePolyPoints(XMLNode* node) {
    if (!node->attributes.Exists("points"))
        return nullptr;

    Token  points_token = node->attributes.Get("points");
    char*  points_text = (char*)malloc(points_token.Length + 1);
    memcpy(points_text, points_token.Start, points_token.Length);
    points_text[points_token.Length] = 0;

    ObjArray* array = NewArray();

    char* token = strtok(points_text, " ");
    while (token != NULL) {
        float fx, fy;
        if (sscanf(token, "%f,%f", &fx, &fy) != 2)
            break;

        ObjArray* sub = NewArray();
        sub->Values->push_back(DECIMAL_VAL(fx));
        sub->Values->push_back(DECIMAL_VAL(fy));
        array->Values->push_back(OBJECT_VAL(sub));

        token = strtok(NULL, " ");
    }

    free(points_text);

    return array;
}

PRIVATE STATIC void TiledMapReader::LoadTileset(XMLNode* tileset, const char* parentFolder) {
    int firstgid = (int)XMLParser::TokenToNumber(tileset->attributes.Get("firstgid"));

    XMLNode* tilesetXML = NULL;
    XMLNode* tilesetNode = NULL;

    char tilesetXMLPath[4096];

    if (tileset->attributes.Exists("source")) {
        Token source = tileset->attributes.Get("source");
        snprintf(tilesetXMLPath, sizeof(tilesetXMLPath), "%s%.*s", parentFolder, (int)source.Length, source.Start);

        tilesetXML = XMLParser::ParseFromResource(tilesetXMLPath);
        if (!tilesetXML)
            return;
        tilesetNode = tilesetXML->children[0];
    }
    else {
        tilesetNode = tileset;
    }

    TileSpriteInfo info;

    for (size_t e = 0; e < tilesetNode->children.size(); e++) {
        if (XMLParser::MatchToken(tilesetNode->children[e]->name, "image")) {
            Token image_source = tilesetNode->children[e]->attributes.Get("source");
            snprintf(tilesetXMLPath, sizeof(tilesetXMLPath), "%s%.*s", parentFolder, (int)image_source.Length, image_source.Start);

            ISprite* tileSprite = new ISprite();
            Scene::TileSprites.push_back(tileSprite);

            int cols, rows;
            tileSprite->Spritesheets[0] = tileSprite->AddSpriteSheet(tilesetXMLPath);
            cols = tileSprite->Spritesheets[0]->Width / Scene::TileSize;
            rows = tileSprite->Spritesheets[0]->Height / Scene::TileSize;

            tileSprite->ReserveAnimationCount(1);
            tileSprite->AddAnimation("TileSprite", 0, 0, cols * rows);

            size_t curTileCount = Scene::TileSpriteInfos.size();
            size_t numEmptyTiles = firstgid - curTileCount;

            if (firstgid == 1)
                numEmptyTiles = 0;

            // Add empty tile
            for (int i = (int)curTileCount; i < firstgid; i++) {
                info.Sprite = tileSprite;
                info.AnimationIndex = 0;
                info.FrameIndex = (int)tileSprite->Animations[0].Frames.size();
                Scene::TileSpriteInfos.push_back(info);

                tileSprite->AddFrame(0, 0, 0, 1, 1, 0, 0);
            }

            // Add tiles
            for (int i = 0; i < cols * rows; i++) {
                info.Sprite = tileSprite;
                info.AnimationIndex = 0;
                info.FrameIndex = (int)tileSprite->Animations[0].Frames.size();
                Scene::TileSpriteInfos.push_back(info);

                tileSprite->AddFrame(0,
                    (i % cols) * Scene::TileSize,
                    (i / cols) * Scene::TileSize,
                    Scene::TileSize, Scene::TileSize, -Scene::TileSize / 2, -Scene::TileSize / 2);
            }

            Tileset sceneTileset(curTileCount + numEmptyTiles, (cols * rows) + (firstgid - curTileCount), tilesetXMLPath);
            Scene::Tilesets.push_back(sceneTileset);
        }
    }

    if (tilesetXML)
        XMLParser::Free(tilesetXML);
}

PUBLIC STATIC void TiledMapReader::Read(const char* sourceF, const char* parentFolder) {
    XMLNode* tileMapXML = XMLParser::ParseFromResource(sourceF);
    if (!tileMapXML) {
        Log::Print(Log::LOG_ERROR, "Could not parse from resource \"%s\"", sourceF);
        return;
    }

    XMLNode* map = tileMapXML->children[0];
    if (!XMLParser::MatchToken(map->attributes.Get("version"), "1.2")) {
        Log::Print(Log::LOG_VERBOSE, "Official support is for Tiled version 1.2; this may still work, however.");
    }

    // 'infinite' maps will not work
    if (XMLParser::MatchToken(map->attributes.Get("infinite"), "1")) {
        Log::Print(Log::LOG_ERROR, "Not compatible with infinite maps! (Map > Map Properties > Set \"Infinite\" to unchecked)");
        XMLParser::Free(tileMapXML);
        return;
    }

    Scene::EmptyTile = 0;
    Scene::TileSize = (int)XMLParser::TokenToNumber(map->attributes.Get("tilewidth"));

    int layer_width = (int)XMLParser::TokenToNumber(map->attributes.Get("width"));
    int layer_height = (int)XMLParser::TokenToNumber(map->attributes.Get("height"));

    for (size_t i = 0; i < map->children.size(); i++) {
        if (XMLParser::MatchToken(map->children[i]->name, "tileset")) {
            XMLNode* tileset = map->children[i];
            TiledMapReader::LoadTileset(tileset, parentFolder);
        }
        else if (XMLParser::MatchToken(map->children[i]->name, "layer")) {
            XMLNode* layer = map->children[i];
            layer_width = (int)XMLParser::TokenToNumber(layer->attributes.Get("width"));
            layer_height = (int)XMLParser::TokenToNumber(layer->attributes.Get("height"));

            size_t layer_size_in_bytes = layer_width * layer_height * sizeof(int);

            int* tile_buffer = NULL;
            HashMap<VMValue>* layer_properties = NULL;

            for (size_t e = 0; e < layer->children.size(); e++) {
                if (XMLParser::MatchToken(layer->children[e]->name, "data")) {
                    XMLNode* data = layer->children[e];
                    XMLNode* data_text = data->children[0];

                    int tile_buffer_len = 0;
                    if (data->attributes.Exists("encoding")) {
                        if (XMLParser::MatchToken(data->attributes.Get("encoding"), "base64")) {
                            tile_buffer = (int*)Memory::Calloc(1, layer_size_in_bytes + 4); // +4 extra space to prevent base64 overflow
                            tile_buffer_len = base64_decode_block(data_text->name.Start, (int)data_text->name.Length, (char*)tile_buffer);
                        }
                        else if (XMLParser::MatchToken(data->attributes.Get("encoding"), "csv")) {
                            Log::Print(Log::LOG_ERROR, "Unsupported tile layer format \"CSV\"!");
                            goto FREE;
                        }
                        else {
                            Log::Print(Log::LOG_ERROR, "Unsupported tile layer format!");
                            goto FREE;
                        }
                    }
                    else {
                        Log::Print(Log::LOG_ERROR, "Unsupported tile layer format \"XML\"!");
                        goto FREE;
                    }

                    if (data->attributes.Exists("compression")) {
                        if (XMLParser::MatchToken(data->attributes.Get("compression"), "zlib")) {
                            int* decomp_tile_buffer = (int*)Memory::Malloc(layer_size_in_bytes);
                            ZLibStream::Decompress(decomp_tile_buffer, layer_size_in_bytes, tile_buffer, tile_buffer_len);
                            Memory::Free(tile_buffer);

                            tile_buffer = decomp_tile_buffer;
                        }
                        else {
                            Log::Print(Log::LOG_ERROR, "Unsupported tile layer compression format!");
                            goto FREE;
                        }
                    }
                }
                else if (XMLParser::MatchToken(layer->children[e]->name, "properties")) {
                    XMLNode* properties = layer->children[e];
                    for (size_t pr = 0; pr < properties->children.size(); pr++) {
                        if (!XMLParser::MatchToken(properties->children[pr]->name, "property"))
                            continue;

                        if (layer_properties == NULL)
                            layer_properties = new HashMap<VMValue>(NULL, 4);

                        TiledMapReader::ParsePropertyNode(properties->children[pr], layer_properties);
                    }
                }
            }

            Token name = layer->attributes.Get("name");

            SceneLayer scenelayer(layer_width, layer_height);
            strncpy(scenelayer.Name, name.Start, name.Length);
            scenelayer.Name[name.Length] = 0;

            scenelayer.RelativeY = 0x100;
            scenelayer.ConstantY = 0x00;
            scenelayer.Flags = SceneLayer::FLAGS_COLLIDEABLE | SceneLayer::FLAGS_NO_REPEAT_X | SceneLayer::FLAGS_NO_REPEAT_Y;
            scenelayer.DrawGroup = 0;
            scenelayer.Properties = layer_properties;

            if (layer->attributes.Exists("visible") && XMLParser::MatchToken(layer->attributes.Get("visible"), "0")) {
                scenelayer.Visible = false;
            }
            if (layer->attributes.Exists("opacity")) {
                scenelayer.Blending = true;
                scenelayer.Opacity = XMLParser::TokenToNumber(layer->attributes.Get("opacity"));
            }

            // NOTE: This makes all tiles Full Solid by default,
            //   so that they can be altered by an object later.
            int what_u_need = TILE_FLIPX_MASK | TILE_FLIPY_MASK | TILE_IDENT_MASK;
            for (size_t i = 0, iSz = layer_size_in_bytes / 4; i < iSz; i++) {
                tile_buffer[i] &= what_u_need;
                tile_buffer[i] |= TILE_COLLA_MASK;
                tile_buffer[i] |= TILE_COLLB_MASK;
            }

            // Fills the tiles from the buffer
            for (int i = 0, iH = 0; i < layer_height; i++) {
                memcpy(&scenelayer.Tiles[iH], &tile_buffer[i * layer_width], layer_width * sizeof(int));
                iH += scenelayer.WidthData;
            }
            memcpy(scenelayer.TilesBackup, scenelayer.Tiles, scenelayer.DataSize);


            // Create parallax data
            scenelayer.ScrollInfoCount = 1;
            scenelayer.ScrollInfos = (ScrollingInfo*)Memory::Malloc(scenelayer.ScrollInfoCount * sizeof(ScrollingInfo));
            for (int g = 0; g < scenelayer.ScrollInfoCount; g++) {
                scenelayer.ScrollInfos[g].RelativeParallax = 0x0100;
                scenelayer.ScrollInfos[g].ConstantParallax = 0x0000;
                scenelayer.ScrollInfos[g].CanDeform = false;
            }

            Scene::Layers.push_back(scenelayer);

            Memory::Free(tile_buffer);
        }
        else if (XMLParser::MatchToken(map->children[i]->name, "objectgroup")) {
            XMLNode* objectgroup = map->children[i];
            for (size_t o = 0; o < objectgroup->children.size(); o++) {
                XMLNode* object = objectgroup->children[o];
                Token    object_type = object->attributes.Get("name");
                float    object_x = XMLParser::TokenToNumber(object->attributes.Get("x"));
                float    object_y = XMLParser::TokenToNumber(object->attributes.Get("y"));
                char     object_type_string[256];

                strncpy(object_type_string, object_type.Start, object_type.Length);
                object_type_string[object_type.Length] = 0;

                ObjectList* objectList = Scene::GetStaticObjectList(object_type_string);
                if (objectList->SpawnFunction) {
                    BytecodeObject* obj = (BytecodeObject*)objectList->Spawn();
                    obj->X = object_x;
                    obj->Y = object_y;
                    obj->InitialX = obj->X;
                    obj->InitialY = obj->Y;
                    obj->List = objectList;
                    Scene::AddStatic(objectList, obj);

                    if (object->attributes.Exists("id"))
                        obj->SlotID = (int)XMLParser::TokenToNumber(object->attributes.Get("id"));

                    if (object->attributes.Exists("width") &&
                        object->attributes.Exists("height")) {
                        obj->Properties->Put("Width", INTEGER_VAL((int)XMLParser::TokenToNumber(object->attributes.Get("width"))));
                        obj->Properties->Put("Height", INTEGER_VAL((int)XMLParser::TokenToNumber(object->attributes.Get("height"))));
                    }
                    if (object->attributes.Exists("rotation")) {
                        obj->Properties->Put("Rotation", INTEGER_VAL((int)XMLParser::TokenToNumber(object->attributes.Get("rotation"))));
                    }

                    if (object->attributes.Exists("gid")) {
                        Uint32 gid = (Uint32)XMLParser::TokenToNumber(object->attributes.Get("gid"));
                        if (gid & TILE_FLIPX_MASK)
                            obj->Properties->Put("FlipX", INTEGER_VAL(1));
                        else
                            obj->Properties->Put("FlipX", INTEGER_VAL(0));
                        if (gid & TILE_FLIPY_MASK)
                            obj->Properties->Put("FlipY", INTEGER_VAL(1));
                        else
                            obj->Properties->Put("FlipY", INTEGER_VAL(0));
                    }

                    for (size_t p = 0; p < object->children.size(); p++) {
                        XMLNode* child = object->children[p];

                        if (XMLParser::MatchToken(child->name, "properties")) {
                            for (size_t pr = 0; pr < child->children.size(); pr++) {
                                if (XMLParser::MatchToken(child->children[pr]->name, "property"))
                                    TiledMapReader::ParsePropertyNode(child->children[pr], obj->Properties);
                            }
                        } else if (XMLParser::MatchToken(child->name, "polygon")) {
                            ObjArray* points = TiledMapReader::ParsePolyPoints(child);
                            obj->Properties->Put("PolygonPoints", OBJECT_VAL(points));
                        } else if (XMLParser::MatchToken(child->name, "polyline")) {
                            ObjArray* points = TiledMapReader::ParsePolyPoints(child);
                            obj->Properties->Put("LinePoints", OBJECT_VAL(points));
                        }
                    }
                }
            }
        }
        else if (XMLParser::MatchToken(map->children[i]->name, "properties")) {
            XMLNode* properties = map->children[i];
            for (size_t pr = 0; pr < properties->children.size(); pr++) {
                if (!XMLParser::MatchToken(properties->children[pr]->name, "property"))
                    continue;

                if (Scene::Properties == NULL)
                    Scene::Properties = new HashMap<VMValue>(NULL, 4);

                TiledMapReader::ParsePropertyNode(properties->children[pr], Scene::Properties);
            }
        }
    }

    FREE:
    XMLParser::Free(tileMapXML);
}
