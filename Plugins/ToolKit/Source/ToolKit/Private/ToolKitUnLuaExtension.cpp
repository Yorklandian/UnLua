#pragma warning(disable : 4996)

#if TOOLKIT_SUPPORT_UNLUA
#include "ToolKitUnLuaExtension.h"

#include <Containers/LuaArray.h>
#include <Containers/LuaMap.h>
#include <Containers/LuaSet.h>
#include <Modules/ModuleManager.h>
#include <ReflectionUtils/PropertyCreator.h>
#include <ReflectionUtils/PropertyDesc.h>
//#include <ReflectionUtils/ReflectionRegistry.h>

#include "ToolKit.h"

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#if PLATFORM_WINDOWS
#undef GetObject
#endif

namespace FToolKitUnLuaExtension
{

    void OnLuaStateCreated(lua_State* L)
    {
        FToolKitModule* ToolKitModuleInstance = FModuleManager::GetModulePtr<FToolKitModule>("ToolKit");

        if (ToolKitModuleInstance != NULL && L != NULL)
        {
            ToolKitModuleInstance->AttachLuaState(L);
        }
    }

    void OnPreLuaContextCleanup(bool bFullClean)
    {
        FToolKitModule* ToolKitModuleInstance = FModuleManager::GetModulePtr<FToolKitModule>("ToolKit");

        if (ToolKitModuleInstance != NULL && bFullClean)
        {
            ToolKitModuleInstance->DetachLuaState();
        }
    }

    static void GetUPropertyValue(Variable* LuaValue, int32 Depth, const UProperty* Property, const void* ValuePtr, bool bIsKey = false);

    static void GetUStructValue(Variable* LuaValue, int32 Depth, const UStruct* Struct, const void* ContainerPtr, const UObjectBaseUtility* ContainerObject = NULL, bool bIsKey = false);

    static void GetTArrayValue(Variable* LuaValue, int32 Depth, const UProperty* Property, FScriptArrayHelper& ArrayHelper, bool bIsKey = false)
    {
        if (LuaValue == NULL || Property == NULL)
            return;
        else if (Depth < 0)
            return;

        char Buffer[256] = { 0 };

        FString InnerExtendedTypeText;
        FString InnerTypeText = Property->GetCPPType(&InnerExtendedTypeText);

        sprintf(Buffer, "TArray<%s%s> Num=%d",
            TCHAR_TO_UTF8(*InnerTypeText),
            TCHAR_TO_UTF8(*InnerExtendedTypeText), ArrayHelper.Num());
        if (bIsKey)
            LuaValue->name = Buffer;
        else
        {
            LuaValue->value = Buffer;
            LuaValue->value_type = LUA_TUSERDATA;
        }

        if (Depth < 1 || bIsKey)
            return;

        for (int32 i = 0; i < ArrayHelper.Num(); i++)
        {
            Variable* Child = LuaValue->children.emplace_back();
            if (Child != NULL)
            {
                sprintf(Buffer, "[%d]", i + 1);
                Child->name = Buffer;
            }
            GetUPropertyValue(Child, Depth - 1, Property, ArrayHelper.GetRawPtr(i));
        }
    }

    static void GetTMapValue(Variable* LuaValue, int32 Depth, FScriptMapHelper& MapHelper, bool bIsKey = false)
    {
        if (LuaValue == NULL)
            return;

        char Buffer[256] = { 0 };

        FString KeyExtendedTypeText, ValueExtendedTypeText, ValueTypeText;
        FString KeyTypeText = MapHelper.KeyProp->GetCPPType(&KeyExtendedTypeText);

        if (MapHelper.ValueProp->GetPropertyFlags() & CPF_ComputedFlags)
        {
            ValueTypeText = FString(TEXT("not cpp type"));
        }
        else
        {
            ValueTypeText = MapHelper.ValueProp->GetCPPType(&ValueExtendedTypeText);
        }

        sprintf(Buffer, "TMap<%s%s,%s%s> Num=%d",
            TCHAR_TO_UTF8(*KeyTypeText), TCHAR_TO_UTF8(*KeyExtendedTypeText),
            TCHAR_TO_UTF8(*ValueTypeText), TCHAR_TO_UTF8(*ValueExtendedTypeText), MapHelper.Num());
        if (bIsKey)
            LuaValue->name = Buffer;
        else
        {
            LuaValue->value = Buffer;
            LuaValue->value_type = LUA_TUSERDATA;
        }

        if (Depth < 1 || bIsKey)
            return;

        for (int i = 0, j = 0; j < MapHelper.Num(); i++)
        {
            if (MapHelper.IsValidIndex(i))
            {
                ++j;

                Variable* Child = LuaValue->children.emplace_back();

                GetUPropertyValue(Child, Depth - 1, MapHelper.KeyProp, MapHelper.GetKeyPtr(i), true);
                GetUPropertyValue(Child, Depth - 1, MapHelper.ValueProp, MapHelper.GetValuePtr(i), false);
            }
        }
    }

    static void GetTSetValue(Variable* LuaValue, int32 Depth, FScriptSetHelper& SetHelper, bool bIsKey = false)
    {
        if (LuaValue == NULL)
            return;

        char Buffer[256] = { 0 };

        FString ElementExtendedTypeText;
        FString ElementTypeText = SetHelper.ElementProp->GetCPPType(&ElementExtendedTypeText);

        sprintf(Buffer, "TSet<%s%s> Num=%d",
            TCHAR_TO_UTF8(*ElementTypeText),
            TCHAR_TO_UTF8(*ElementExtendedTypeText), SetHelper.Num());
        if (bIsKey)
            LuaValue->name = Buffer;
        else
        {
            LuaValue->value = Buffer;
            LuaValue->value_type = LUA_TUSERDATA;
        }

        if (Depth < 1 || bIsKey)
            return;

        for (int i = 0, j = 0; j < SetHelper.Num(); i++)
        {
            if (SetHelper.IsValidIndex(i))
            {
                ++j;

                Variable* Child = LuaValue->children.emplace_back();
                if (Child != NULL)
                {
                    sprintf(Buffer, "[%d]", j);
                    Child->name = Buffer;
                }

                GetUPropertyValue(Child, Depth - 1, SetHelper.ElementProp, SetHelper.GetElementPtr(i));
            }
        }
    }

    static void GetUPropertyValue(Variable* LuaValue, int32 Depth, const UProperty* Property, const void* ValuePtr, bool bIsKey)
    {
        if (LuaValue == NULL || Property == NULL || ValuePtr == NULL)
            return;

        char Buffer[256] = { 0 };

        int32 Type = GetPropertyType(Property);
        switch (Type)
        {
        case CPT_Int8:
        case CPT_Int16:
        case CPT_Int:
        case CPT_Int64:
        case CPT_Byte:
        case CPT_UInt16:
        case CPT_UInt32:
        case CPT_UInt64:
        {
            FNumericProperty* NumericProperty = (FNumericProperty*)Property;

            sprintf(Buffer, "%lld",
                NumericProperty->GetUnsignedIntPropertyValue(ValuePtr));
            if (bIsKey)
                LuaValue->name = Buffer;
            else
                LuaValue->value = Buffer;
            break;
        }
        case CPT_Float:
        case CPT_Double:
        {
            FNumericProperty* NumericProperty = (FNumericProperty*)Property;

            sprintf(Buffer, "%lf",
                NumericProperty->GetFloatingPointPropertyValue(ValuePtr));
            if (bIsKey)
                LuaValue->name = Buffer;
            else
                LuaValue->value = Buffer;
            break;
        }
        case CPT_Enum:
        {
            FEnumProperty* EnumProperty = (FEnumProperty*)Property;

            int64 EnumValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
            sprintf(Buffer, "%s: %s",
                TCHAR_TO_UTF8(*EnumProperty->GetCPPType(NULL, 0)),
                TCHAR_TO_UTF8(*EnumProperty->GetEnum()->GetNameStringByValue(EnumValue)));
            if (bIsKey)
                LuaValue->name = Buffer;
            else
                LuaValue->value = Buffer;
            break;
        }
        case CPT_Bool:
        {
            FBoolProperty* BoolProperty = (FBoolProperty*)Property;

            sprintf(Buffer, "%s",
                BoolProperty->GetPropertyValue(ValuePtr) ? "true" : "false");
            if (bIsKey)
                LuaValue->name = Buffer;
            else
                LuaValue->value = Buffer;
            break;
        }
        case CPT_ObjectReference:
        {
            FObjectProperty* ObjectProperty = (FObjectProperty*)Property;

            if (ObjectProperty->PropertyClass->IsChildOf(UClass::StaticClass()))
            {
                FClassProperty* ClassProperty = (FClassProperty*)ObjectProperty;

                UClass* Class = Cast<UClass>(ObjectProperty->GetPropertyValue(ValuePtr));
                UClass* MetaClass = Class ? Class : ClassProperty->MetaClass;
                if (MetaClass == UClass::StaticClass())
                {
                    sprintf(Buffer, "UClass*: 0x%p", Class);
                    if (bIsKey)
                        LuaValue->name = Buffer;
                    else
                        LuaValue->value = Buffer;

                }
                else
                {
                    sprintf(Buffer, "TSubclassOf<%s%s>(0x%p)",
                        TCHAR_TO_UTF8(MetaClass->GetPrefixCPP()),
                        TCHAR_TO_UTF8(*MetaClass->GetName()), Class);
                    if (bIsKey)
                        LuaValue->name = Buffer;
                    else
                        LuaValue->value = Buffer;
                }
            }
            else
            {
                UObject* Object = ObjectProperty->GetPropertyValue(ValuePtr);
                UClass* Class = Object ? Object->GetClass() : ObjectProperty->PropertyClass;

                sprintf(Buffer, "%s%s* (%s)(0x%p)", TCHAR_TO_UTF8(Class->GetPrefixCPP()),
                    TCHAR_TO_UTF8(*Class->GetName()),
                    Object ? TCHAR_TO_UTF8(*Object->GetName()) : "", Object);
                if (bIsKey)
                    LuaValue->name = Buffer;
                else
                {
                    LuaValue->value = Buffer;
                    LuaValue->value_type = LUA_TUSERDATA;
                }

                if (Depth >= 0 && !bIsKey)
                {
                    GetUStructValue(LuaValue, Depth - 1, Class, Object, Object);
                }
            }
            break;
        }
        case CPT_WeakObjectReference:  /* Serial number and index (in `GUObjectArray`) of the object. */
        {
            FWeakObjectProperty* WeakObjectProperty = (FWeakObjectProperty*)Property;
            const FWeakObjectPtr& WeakObject = WeakObjectProperty->GetPropertyValue(ValuePtr);

            UObject* Object = WeakObject.Get();
            UClass* Class = Object ? Object->GetClass() : NULL;

            sprintf(Buffer, "%s(0x%p)",
                TCHAR_TO_UTF8(*WeakObjectProperty->GetCPPType(NULL, 0)), Object);
            if (bIsKey)
                LuaValue->name = Buffer;
            else
            {
                LuaValue->value = Buffer;
                LuaValue->value_type = LUA_TUSERDATA;
            }

            if (Depth >= 0 && !bIsKey)
            {
                GetUStructValue(LuaValue, Depth - 1, Class, Object, Object);
            }
            break;
        }
        case CPT_LazyObjectReference:  /* GUID of the object. */
        {
            FLazyObjectProperty* LazyObjectProperty = (FLazyObjectProperty*)Property;
            const FLazyObjectPtr& LazyObject = LazyObjectProperty->GetPropertyValue(ValuePtr);

            UObject* Object = LazyObject.Get();
            UClass* Class = Object ? Object->GetClass() : NULL;

            sprintf(Buffer, "%s: %s(0x%p)",
                TCHAR_TO_UTF8(*LazyObjectProperty->GetCPPType(NULL, 0)),
                TCHAR_TO_UTF8(*LazyObject.GetUniqueID().ToString()), Object);
            if (bIsKey)
                LuaValue->name = Buffer;
            else
            {
                LuaValue->value = Buffer;
                LuaValue->value_type = LUA_TUSERDATA;
            }

            if (Depth >= 0 && !bIsKey)
            {
                GetUStructValue(LuaValue, Depth - 1, Class, Object, Object);
            }
            break;
        }
        case CPT_SoftObjectReference:  /* Path of the object. */
        {
            FSoftObjectProperty* SoftObjectProperty = (FSoftObjectProperty*)Property;
            const FSoftObjectPtr& SoftObject = SoftObjectProperty->GetPropertyValue(ValuePtr);

            UObject* Object = SoftObject.Get();
            UClass* Class = Object ? Object->GetClass() : NULL;

            sprintf(Buffer, "%s: %s(0x%p)",
                TCHAR_TO_UTF8(*SoftObjectProperty->GetCPPType(NULL, 0)),
                TCHAR_TO_UTF8(*SoftObject.ToString()), Object);
            if (bIsKey)
                LuaValue->name = Buffer;
            else
            {
                LuaValue->value = Buffer;
                LuaValue->value_type = LUA_TUSERDATA;
            }

            if (Depth >= 0 && !bIsKey)
            {
                GetUStructValue(LuaValue, Depth - 1, Class, Object, Object);
            }
            break;
        }
        case CPT_Interface:
        {
            FInterfaceProperty* InterfaceProperty = (FInterfaceProperty*)Property;

            FString InterfaceExtendedTypeText;
            FString InterfaceTypeText = InterfaceProperty->GetCPPType(&InterfaceExtendedTypeText, 0);
            const FScriptInterface& Interface = InterfaceProperty->GetPropertyValue(ValuePtr);

            UObject* Object = Interface.GetObject();
            UClass* Class = Object ? Object->GetClass() : NULL;

            sprintf(Buffer, "%s%s: Object(0x%p), Interface(0x%p)",
                TCHAR_TO_UTF8(*InterfaceTypeText),
                TCHAR_TO_UTF8(*InterfaceExtendedTypeText), Object,
                Interface.GetInterface());
            if (bIsKey)
                LuaValue->name = Buffer;
            else
            {
                LuaValue->value = Buffer;
                LuaValue->value_type = LUA_TUSERDATA;
            }

            if (Depth >= 0 && !bIsKey)
            {
                GetUStructValue(LuaValue, Depth - 1, Class, Object, Object);
            }
            break;
        }
        case CPT_Name:
        {
            FNameProperty* NameProperty = (FNameProperty*)Property;

            sprintf(Buffer, "%s",
                TCHAR_TO_UTF8(*NameProperty->GetPropertyValue(ValuePtr).ToString()));
            if (bIsKey)
                LuaValue->name = Buffer;
            else
                LuaValue->value = Buffer;
            break;
        }
        case CPT_String:
        {
            FStrProperty* StringProperty = (FStrProperty*)Property;

            sprintf(Buffer, "%s",
                TCHAR_TO_UTF8(*StringProperty->GetPropertyValue(ValuePtr)));
            if (bIsKey)
                LuaValue->name = Buffer;
            else
                LuaValue->value = Buffer;
            break;
        }
        case CPT_Text:
        {
            FTextProperty* TextProperty = (FTextProperty*)Property;

            sprintf(Buffer, "%s",
                TCHAR_TO_UTF8(*TextProperty->GetPropertyValue(ValuePtr).ToString()));
            if (bIsKey)
                LuaValue->name = Buffer;
            else
                LuaValue->value = Buffer;
            break;
        }
        case CPT_Array:
        {
            FArrayProperty* ArrayProperty = (FArrayProperty*)Property;
            FScriptArray* ScriptArray = (FScriptArray*)(&ArrayProperty->GetPropertyValue(ValuePtr));

            FScriptArrayHelper ArrayHelper(ArrayProperty, ScriptArray);
            GetTArrayValue(LuaValue, Depth, ArrayProperty->Inner, ArrayHelper, bIsKey);
            break;
        }
        case CPT_Map:
        {
            FMapProperty* MapProperty = (FMapProperty*)Property;
            FScriptMap* ScriptMap = (FScriptMap*)(&MapProperty->GetPropertyValue(ValuePtr));

            FScriptMapHelper MapHelper(MapProperty, ScriptMap);
            GetTMapValue(LuaValue, Depth, MapHelper, bIsKey);
            break;
        }
        case CPT_Set:
        {
            FSetProperty* SetProperty = (FSetProperty*)Property;
            FScriptSet* ScriptSet = (FScriptSet*)(&SetProperty->GetPropertyValue(ValuePtr));

            FScriptSetHelper SetHelper(SetProperty, ScriptSet);
            GetTSetValue(LuaValue, Depth, SetHelper, bIsKey);
            break;
        }
        case CPT_Struct:
        {
            FStructProperty* StructProperty = (FStructProperty*)Property;

            sprintf(Buffer, "%s(0x%p)",
                TCHAR_TO_UTF8(*StructProperty->GetCPPType(nullptr, 0)), ValuePtr);
            if (bIsKey)
                LuaValue->name = Buffer;
            else
            {
                LuaValue->value = Buffer;
                LuaValue->value_type = LUA_TUSERDATA;
            }

            if (Depth >= 0 && !bIsKey)
            {
                GetUStructValue(LuaValue, Depth - 1, StructProperty->Struct, ValuePtr);
            }
            break;
        }
        default:
        {
            sprintf(Buffer, "Non-supported Type : %d", Type);
            LuaValue->value = Buffer;
            break;
        }
        }
    }

    static void GetUStructValue(Variable* LuaValue, int32 Depth, const UStruct* Struct, const void* ContainerPtr, const UObjectBaseUtility* ContainerObject, bool bIsKey)
    {
        if (LuaValue == NULL || Struct == NULL || ContainerPtr == NULL)
            return;
        else if (Depth < 0)
            return;

        if (ContainerObject != NULL && ContainerObject->HasAnyFlags(RF_NeedInitialization))
            return;

        for (UProperty* Property = Struct->PropertyLink; Property != NULL; Property = Property->PropertyLinkNext)
        {
            if (Property->HasAnyPropertyFlags(CPF_Deprecated))
            {
                continue;
            }

            Variable* Child = LuaValue->children.emplace_back();
            if (Child != NULL)
            {
                char Buffer[256] = { 0 };
                sprintf(Buffer, "%s", TCHAR_TO_UTF8(*Property->GetNameCPP()));
                Child->name = Buffer;
            }

            GetUPropertyValue(Child, Depth - 1,
                Property, Property->ContainerPtrToValuePtr<void>(ContainerPtr));
        }
    }

    void UnLuaUserdataEvaluator(lua_State* L, Variable* LuaValue, int32 Index, int32 Depth)
    {
        if (L == NULL || LuaValue == NULL || lua_type(L, Index) != LUA_TUSERDATA)
            return;
        else if (Depth < 0)
            return;

        void* ContainerPtr = UnLua::GetPointer(L, Index);

        const char* ClassNamePtr = NULL;
        if (lua_getmetatable(L, Index) != 0)
        {
            lua_pushstring(L, "__name");
            lua_rawget(L, -2);

            if (lua_isstring(L, -1))
            {
                ClassNamePtr = lua_tostring(L, -1);
            }
            lua_pop(L, 2);
        }

        if (ClassNamePtr != NULL)
        {
            FString ClassName(ClassNamePtr);

            UStruct* Struct = FindObject<UStruct>(ANY_PACKAGE, *ClassName + 1);
            if (Struct != NULL)
            {
                UObjectBaseUtility* ContainerObject = NULL;

                UClass* Class = Cast<UClass>(Struct);
                if (Class != NULL)
                {
                    if (Class->IsChildOf(UClass::StaticClass()))
                    {
                        UClass* MetaClass = (ContainerPtr != NULL) ? (UClass*)ContainerPtr : Class;
                        if (MetaClass == UClass::StaticClass())
                        {
                            char Buffer[256] = { 0 };
                            sprintf(Buffer, "UClass*(0x%p)", ContainerPtr);
                            LuaValue->value = Buffer;
                        }
                        else
                        {
                            char Buffer[256] = { 0 };
                            sprintf(Buffer, "TSubclassOf<%s%s>(0x%p)",
                                TCHAR_TO_UTF8(MetaClass->GetPrefixCPP()),
                                TCHAR_TO_UTF8(*MetaClass->GetName()), ContainerPtr);
                            LuaValue->value = Buffer;
                        }
                    }
                    else
                    {
                        UObject* Object = (UObject*)ContainerPtr;
                        ContainerObject = Object;

                        char Buffer[256] = { 0 };
                        sprintf(
                            Buffer, "%s%s* (%s: 0x%p)",
                            TCHAR_TO_UTF8(Class->GetPrefixCPP()),
                            TCHAR_TO_UTF8(*Class->GetName()),
                            Object ? TCHAR_TO_UTF8(*Object->GetName()) : "", ContainerPtr);
                        LuaValue->value = Buffer;
                    }
                }
                else
                {
                    UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Struct);
                    if (ScriptStruct != NULL)
                    {
                        char Buffer[256] = { 0 };
                        sprintf(Buffer, "F%s(0x%p)",
                            TCHAR_TO_UTF8(*ScriptStruct->GetName()), ContainerPtr);
                        LuaValue->value = Buffer;
                    }
                    else
                    {
                        char Buffer[256] = { 0 };
                        sprintf(Buffer, "userdata %s(0x%p)",
                            TCHAR_TO_UTF8(*ClassName), ContainerPtr);
                        LuaValue->value = Buffer;
                        return;
                    }
                }

                LuaValue->value_type = LUA_TUSERDATA;
                GetUStructValue(LuaValue, Depth - 1, Struct, ContainerPtr, ContainerObject);
            }
            else
            {
                if (ContainerPtr == NULL)
                {
                    char Buffer[256] = { 0 };
                    sprintf(Buffer, "userdata %s(0x%p)",
                        TCHAR_TO_UTF8(*ClassName), ContainerPtr);
                    LuaValue->value = Buffer;
                    return;
                }

                if (ClassName == TEXT("TArray"))
                {
                    FLuaArray* Array = (FLuaArray*)ContainerPtr;

                    FProperty* Property = Array->Inner->GetUProperty();
                    if (Property != NULL)
                    {
                        FScriptArrayHelper ArrayHelper = FScriptArrayHelper::CreateHelperFormInnerProperty(
                            Property, Array->ScriptArray);
                        GetTArrayValue(LuaValue, Depth, Property, ArrayHelper);
                    }
                    else
                    {
                        char Buffer[256] = { 0 };
                        sprintf(Buffer, "TArray<UnknownType> Num=%d", Array->Num());
                        LuaValue->value = Buffer;
                    }
                }
                else if (ClassName == TEXT("TMap"))
                {
                    FLuaMap* Map = (FLuaMap*)ContainerPtr;

                    FProperty* KeyProperty = Map->KeyInterface->GetUProperty();
                    FProperty* ValueProperty = Map->ValueInterface->GetUProperty();
                    if (KeyProperty != NULL && ValueProperty != NULL)
                    {
                        FScriptMapHelper MapHelper = FScriptMapHelper::CreateHelperFormInnerProperties(
                            KeyProperty, ValueProperty, Map->Map);
                        GetTMapValue(LuaValue, Depth, MapHelper);
                    }
                    else
                    {
                        char Buffer[256] = { 0 };
                        sprintf(Buffer, "TMap<UnknownType, UnknownType> Num=%d", Map->Num());
                        LuaValue->value = Buffer;
                    }
                }
                else if (ClassName == TEXT("TSet"))
                {
                    FLuaSet* Set = (FLuaSet*)ContainerPtr;

                    FProperty* ElementProperty = Set->ElementInterface->GetUProperty();
                    if (ElementProperty != NULL)
                    {
                        FScriptSetHelper SetHelper = FScriptSetHelper::CreateHelperFormElementProperty(
                            ElementProperty, Set->Set);
                        GetTSetValue(LuaValue, Depth, SetHelper);
                    }
                    else
                    {
                        char Buffer[256] = { 0 };
                        sprintf(Buffer, "TSet<UnknownType> Num=%d", Set->Num());
                        LuaValue->value = Buffer;
                    }
                }
                else if (ClassName == TEXT("FSoftObjectPtr"))
                {
                    FSoftObjectPtr* SoftObject = (FSoftObjectPtr*)ContainerPtr;

                    char Buffer[256] = { 0 };
                    sprintf(Buffer, "TSoftObjectPtr<UObject>: %s(0x%p)",
                        TCHAR_TO_UTF8(*SoftObject->ToString()), SoftObject->Get());
                    LuaValue->value = Buffer;
                }
                else
                {
                    char Buffer[256] = { 0 };
                    sprintf(Buffer, "userdata %s(0x%p)",
                        TCHAR_TO_UTF8(*ClassName), ContainerPtr);
                    LuaValue->value = Buffer;
                }
            }
        }
        else
        {
            char Buffer[256] = { 0 };
            sprintf(Buffer, "userdata(0x%p)", ContainerPtr);
            LuaValue->value = Buffer;
        }
    }

}
#endif