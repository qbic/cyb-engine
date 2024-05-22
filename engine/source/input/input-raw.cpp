#include "input/input.h"
#include "input/input-raw.h"
#include "core/logger.h"
#include <hidsdi.h>

#pragma comment(lib, "Hid.lib")

namespace cyb::input::rawinput
{
	// Simple LinearAllocator that will enforce data alignment
	class AlignedLinearAllocator
	{
	public:
		~AlignedLinearAllocator()
		{
			_aligned_free(buffer);
		}

		[[nodiscard]] constexpr size_t GetCapacity() const
		{
			return capacity;
		}

		inline void Reserve(size_t newCapacity, size_t align)
		{
			alignment = align;
			newCapacity = Align(newCapacity, alignment);
			capacity = newCapacity;

			_aligned_free(buffer);
			buffer = (uint8_t*)_aligned_malloc(capacity, alignment);
		}

		[[nodiscard]] constexpr uint8_t* Allocate(size_t size)
		{
			size = Align(size, alignment);
			if (offset + size <= capacity)
			{
				uint8_t* ret = &buffer[offset];
				offset += size;
				return ret;
			}
			return nullptr;
		}

		constexpr void Free(size_t size)
		{
			size = Align(size, alignment);
			assert(offset >= size);
			offset -= size;
		}

		constexpr void Reset()
		{
			offset = 0;
		}

	private:
		uint8_t* buffer = nullptr;
		size_t capacity = 0;
		size_t offset = 0;
		size_t alignment = 1;

		[[nodiscard]] constexpr size_t Align(size_t value, size_t alignment)
		{
			return ((value + alignment - 1) / alignment) * alignment;
		}
	};

	AlignedLinearAllocator allocator;
	std::vector<RAWINPUT *> inputMessages;

	input::KeyboardState keyboard;
    input::MouseState mouse;
	std::atomic<bool> initialized = false;

    void Initialize()
    {
        RAWINPUTDEVICE rid[2];

        // Register mouse:
        rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        rid[0].dwFlags = 0;
        rid[0].hwndTarget = 0;

        // Register keyboard:
        rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
        rid[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
        rid[1].dwFlags = 0;
        rid[1].hwndTarget = 0;

        if (RegisterRawInputDevices(rid, (UINT)CountOf(rid), sizeof(rid[0])) == FALSE)
        {
            // registration failed. Call GetLastError for the cause of the error
            assert(0);
        }

		allocator.Reserve(1024 * 1024, 8); // 1Mb temp buffer, 8byte alignment
		inputMessages.reserve(64);
		initialized.store(true);
    }

	void ParseRawInputBlock(const RAWINPUT& raw)
	{
		if (raw.header.dwType == RIM_TYPEKEYBOARD)
		{
			const RAWKEYBOARD& rawkeyboard = raw.data.keyboard;
			assert(rawkeyboard.VKey < _countof(keyboard.buttons));

			if (rawkeyboard.Flags == RI_KEY_MAKE)
			{
				keyboard.buttons[rawkeyboard.VKey].RegisterKeyDown();
			}
			else if (rawkeyboard.Flags == RI_KEY_BREAK)
			{
				keyboard.buttons[rawkeyboard.VKey].RegisterKeyUp();
			}
		}
		else if (raw.header.dwType == RIM_TYPEMOUSE)
		{
			const RAWMOUSE& rawmouse = raw.data.mouse;

			if (rawmouse.usFlags == MOUSE_MOVE_RELATIVE)
			{
				if (std::abs(rawmouse.lLastX) < 30000)
				{
					mouse.deltaPosition.x += (float)rawmouse.lLastX;
				}
				if (std::abs(rawmouse.lLastY) < 3000)
				{
					mouse.deltaPosition.y += (float)rawmouse.lLastY;
				}
				if (rawmouse.usButtonFlags == RI_MOUSE_WHEEL)
				{
					mouse.deltaWheel += float((SHORT)rawmouse.usButtonData) / float(WHEEL_DELTA);
				}
			}
			else if (rawmouse.usFlags == MOUSE_MOVE_ABSOLUTE)
			{
				// for some reason we never get absolute coordinates with raw input...
			}

			if (rawmouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_DOWN)
			{
				mouse.leftButton.RegisterKeyDown();
			}
			else if (rawmouse.usButtonFlags == RI_MOUSE_LEFT_BUTTON_UP)
			{
				mouse.leftButton.RegisterKeyUp();
			}
			else if (rawmouse.usButtonFlags == RI_MOUSE_MIDDLE_BUTTON_DOWN)
			{
				mouse.middleButton.RegisterKeyDown();
			}
			else if (rawmouse.usButtonFlags == RI_MOUSE_MIDDLE_BUTTON_UP)
			{
				mouse.middleButton.RegisterKeyUp();
			}
			else if (rawmouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_DOWN)
			{
				mouse.rightButton.RegisterKeyDown();
			}
			else if (rawmouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_UP)
			{
				mouse.rightButton.RegisterKeyUp();
			}
		}
	}

    void Update(bool hasWindowFocus)
    {
		if (hasWindowFocus)
		{
			for (uint32_t i = 0; i < CountOf(keyboard.buttons); ++i)
			{
				keyboard.buttons[i].Reset();
			}

			mouse.position = XMFLOAT2(0, 0);
			mouse.deltaPosition = XMFLOAT2(0, 0);
			mouse.deltaWheel = 0;
			mouse.leftButton.Reset();
			mouse.middleButton.Reset();
			mouse.rightButton.Reset();
		}
		else
		{
			keyboard = input::KeyboardState();
			mouse = input::MouseState();
		}

		// Loop through inputs that we got from text loop
		for (auto& input : inputMessages)
		{
			ParseRawInputBlock(*input);
		}

		inputMessages.clear();
		allocator.Reset();
    }

	void ParseMessage(HRAWINPUT hRawInput)
	{
		if (!initialized.load())
		{
			return;
		}

		UINT dwSize = 0u;

		GetRawInputData(hRawInput, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		BYTE* lpb = allocator.Allocate(dwSize);
		if (lpb == nullptr)
		{
			CYB_WARNING("Input message queue full, dropping input data");
			CYB_DEBUGBREAK();	// (need to increase allocator size if this hits)
			return;
		}

		UINT result = GetRawInputData(hRawInput, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
		if (result == dwSize)
		{
			inputMessages.push_back((RAWINPUT*)lpb);
		}
	}

	void GetKeyboardState(input::KeyboardState* state)
	{
		*state = keyboard;
	}

    void GetMouseState(input::MouseState* state)
    {
        *state = mouse;
    }
}