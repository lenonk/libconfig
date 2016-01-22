
#include <libconfig.h++>

namespace libconfig 
{
	struct Variant
	{

	private:
		bool			isSet;
		Setting::Type	type;

		bool			value_bool;
		int64_t			value_int;
		double			value_float;
		std::string		value_string;

	public:

		Variant()
			: isSet(false)
			, type(Setting::TypeNone)
		{
		}
		Variant(bool value)
		{
			value_bool = value;
			isSet = true;
			type = Setting::TypeBoolean;
		}
		Variant(int32_t value)
		{
			value_int = value;
			isSet = true;
			type = Setting::TypeInt;
		}
		Variant(int64_t value)
		{
			value_int = value;
			isSet = true;
			type = Setting::TypeInt64;
		}
		Variant(double value)
		{
			value_float = value;
			isSet = true;
			type = Setting::TypeFloat;
		}
		Variant(std::string& value)
		{
			value_string = value;
			isSet = true;
			type = Setting::TypeString;
		}
		Variant(const char* value)
		{
			value_string = value;
			isSet = true;
			type = Setting::TypeString;
		}

		operator bool() const { return value_bool; }
		operator int() const { return (int)value_int; }
		operator unsigned int() const { return (unsigned int)value_int; }
		operator long() const { return (long)value_int; }
		operator unsigned long() const { return (unsigned long)value_int; }
		operator long long() const { return (long long)value_int; }
		operator unsigned long long() const { return (unsigned long long)value_int; }
		operator double() const { return value_float; }
		operator float() const { return (float)value_float; }
		operator std::string() const { return value_string; }

		const bool IsSet() const
		{
			return isSet;
		}

		const Setting::Type GetType() const
		{
			return type;
		}
	};

	class ChainedSetting
	{
	public:

		ChainedSetting(Setting& setting, std::ostream& err = std::cerr, ChainedSetting* parent = NULL)
			: name(setting.isRoot() ? "<root>" : (setting.getName() ? setting.getName() : ""))
			, index(setting.getIndex())
			, parent(parent)
			, setting(&setting)
			, err(err)
			, isSettingMandatory(false)
			, anyMandatorySettingIsMissing(false)
		{
		}

		template<typename T>
		ChainedSetting& defaultValue(T defaultValue)
		{
			defaultVal = defaultValue;
			return *this;
		}
		template<typename T>
		ChainedSetting& min(T min)
		{
			minVal = min;
			return *this;
		}
		template<typename T>
		ChainedSetting& max(T max)
		{
			maxVal = max;
			return *this;
		}

		ChainedSetting& isMandatory()
		{
			isSettingMandatory = true;
			if (parent) parent->isMandatory();
			return *this;
		}

		ChainedSetting& comment(std::string comment)
		{
			this->helpComment = comment;
			return *this;
		}

		template<typename T>
		operator T()
		{
			// TODO: test if any variant values mismatch with requested type
			if (!setting)
			{
				if (isSettingMandatory)
				{
					AlertMandatorySettingMissing<T>();
				}

				return GetDefaultValue<T>();
			}

			try
			{
				T value = *setting;
				if (minVal.IsSet())
				{
					T min = minVal;
					if (value < min)
					{
						err << "'" << setting->getPath() << "' setting is out of valid bounds (min: " << min << "). Value was: " << value << std::endl;
						value = min;
					}
				}
				if (maxVal.IsSet())
				{
					T max = maxVal;
					if (value > max)
					{
						err << "'" << setting->getPath() << "' setting is out of valid bounds (max: " << max << "). Value was: " << value << std::endl;
						value = max;
					}
				}
				return value;
			}
			catch (const SettingTypeException& tex)
			{
				err << "'" << tex.getPath() << "' setting is of wrong type." << std::endl;
			}

			return GetDefaultValue<T>();
		}

		ChainedSetting operator[](const char *name)
		{
			if (!setting)
			{
				return ChainedSetting(name, err, this);
			}

			if(setting->exists(name))
			{
				return ChainedSetting((*setting)[name], err, this);
			}
			else
			{
				return ChainedSetting(name, err, this);
			}
		}

		inline ChainedSetting operator[](const std::string &name)
		{
			return(operator[](name.c_str()));
		}

		ChainedSetting operator[](int index)
		{
			if (!setting)
			{
				return ChainedSetting(index, err, this);
			}

			if (index >= 0 && index < setting->getLength())
			{
				return ChainedSetting(setting[index], err, this);
			}
			else
			{
				return ChainedSetting(index, err, this);
			}
		}

		int getLength() const
		{
			return setting ? setting->getLength() : 0;
		}

		bool exists() const
		{
			return setting != NULL;
		}

		bool isAnyMandatorySettingMissing() const
		{
			return anyMandatorySettingIsMissing;
		}

	private: 
		
		ChainedSetting(const std::string& name, std::ostream& err, ChainedSetting* parent)
			: name(name)
			, index(-1)
			, parent(parent)
			, setting(NULL)
			, err(err)
			, isSettingMandatory(false)
			, anyMandatorySettingIsMissing(true)
		{
		}

		ChainedSetting(int index, std::ostream& err, ChainedSetting* parent)
			: name("")
			, index(index)
			, parent(parent)
			, setting(NULL)
			, err(err)
			, isSettingMandatory(false)
			, anyMandatorySettingIsMissing(true)
		{
		}

		std::string GetPath() const
		{
			if (setting)
			{
				return setting->getPath();
			}

			std::string path = (name.length() > 0) ? name : "[" + std::to_string(index) + "]";
			if (parent)
			{
				auto parentPath = parent->GetPath();
				return (parentPath.length() > 0) ? (parentPath + ((name.length() == 0) ? "" : ".") + path) : path;
			}
			return path;
		}

		void PropagateAnyMandatorySettingIsMissing()
		{
			anyMandatorySettingIsMissing = true;
			if (parent)
			{
				parent->PropagateAnyMandatorySettingIsMissing();
			}
		}

		template<typename T>
		void AlertMandatorySettingMissing()
		{
			PropagateAnyMandatorySettingIsMissing();

			err << "Missing '" << GetPath() << "' setting in configuration file.";
			if (helpComment.length() > 0) err << " ("<< helpComment << ")";
			err << std::endl;
		}

		template<typename T>
		T GetUnsetDefaultValue() const
		{
			return (T)0;
		}

		template<>
		std::string GetUnsetDefaultValue() const
		{
			return "";
		}

		template<typename T>
		T GetDefaultValue() const
		{
			if (defaultVal.IsSet())
			{
				return (T)defaultVal;
			}

			return GetUnsetDefaultValue<T>();
		}

		std::string name;
		int index;
		ChainedSetting* parent;
		Setting* setting;
		std::ostream& err;
		Variant defaultVal;
		Variant minVal;
		Variant maxVal;
		std::string helpComment;
		bool isSettingMandatory;
		bool anyMandatorySettingIsMissing;
	};
}