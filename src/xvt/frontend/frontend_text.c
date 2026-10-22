#include "xvt/frontend/frontend_text.h"

#ifdef XVT_MODERN
#include "xvt_runtime/snapshot/render_assets.h"
#include "xvt_runtime/snapshot/render_frontend.h"
#endif
#include "xvt/assets/file.h"
#include "xvt/frontend/front_image.h"
#include "xvt/frontend/frontend.h"
#include "xvt/frontend/frontend_cursor.h"
#include "xvt/frontend/frontend_display.h"
#include "xvt/frontend/frontend_draw.h"
#include "xvt/frontend/frontend_mouse.h"
#include "xvt/frontend/frontend_state.h"
#include "xvt/input/keyboard.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef XVT_MODERN
__declspec(dllimport) void* __stdcall CreateFontA(int height, int width, int escapement, int orientation,
												  int weight, unsigned int italic, unsigned int underline,
												  unsigned int strikeOut, unsigned int charSet,
												  unsigned int outPrecision, unsigned int clipPrecision,
												  unsigned int quality, unsigned int pitchAndFamily,
												  const char* faceName);
__declspec(dllimport) int __stdcall DeleteObject(void* object);
__declspec(dllimport) int __stdcall ExtTextOutA(void* dc, int x, int y, unsigned int options,
												const RECT* rect, const char* text, unsigned int count,
												const int* dx);
__declspec(dllimport) void* __stdcall GetDC(void* hWnd);
__declspec(dllimport) int __stdcall GetTextExtentPoint32A(void* dc, const char* text, int count, SIZE* size);
__declspec(dllimport) int __stdcall ReleaseDC(void* hWnd, void* dc);
__declspec(dllimport) void* __stdcall SelectObject(void* dc, void* object);
__declspec(dllimport) uint32_t __stdcall SetBkColor(void* dc, uint32_t color);
__declspec(dllimport) int __stdcall SetBkMode(void* dc, int mode);
__declspec(dllimport) int __stdcall SetMapMode(void* dc, int mode);
__declspec(dllimport) int __stdcall SetRect(RECT* rect, int left, int top, int right, int bottom);
__declspec(dllimport) uint32_t __stdcall SetTextColor(void* dc, uint32_t color);
__declspec(dllimport) int __stdcall SetTextCharacterExtra(void* dc, int extra);

typedef HRESULT(AERON_DXAPI* FrontendTextSurfaceGetDcFunc)(IDirectDrawSurface* surface, void** dc);
typedef HRESULT(AERON_DXAPI* FrontendTextSurfaceReleaseDcFunc)(IDirectDrawSurface* surface, void* dc);
#endif

// GLOBAL: XVT 0x665684
static int g_savedGlyphScratchTtl;
// GLOBAL: XVT 0x52C000
int g_activeTextFieldId = 0;
// GLOBAL: XVT 0x52C014
static int g_textFieldColor = 0;
// GLOBAL: XVT 0x52C018
static int g_textFieldColorInitialized = 0;
// GLOBAL: XVT 0x665464
static int g_textFieldCursorCharIndex = 0;
// GLOBAL: XVT 0x665680
static int g_textFieldLength = 0;

// FUNCTION: XVT 0x4DA380
int FrontendText_DrawEditableField(RECT* rect, char* text, int maxChars, int fieldId, unsigned int fontSize,
								   const char* ignoredChars) {
	int mouseX;
	int completed;
	int mouseY;
	RECT previousClipRect;
	RECT textClipRect;
	int textWidth;
	int rectWidth;

	if (!g_textFieldColorInitialized) {
		g_textFieldColor = FrontendDisplay_PackRGB(0xFF, 0xFF, 0xFF);
		g_textFieldColorInitialized = 1;
	}
	g_textFieldCursorCharIndex = strlen(text);
	g_textFieldLength = strlen(text);
	completed = 0;
	FrontendCursor_GetPos(&mouseX, &mouseY);
	if (FrontendDraw_PointInRect(rect, mouseX, mouseY) &&
		(FrontendMouse_GetLeftClick() || FrontendMouse_GetRightClick())) {
		g_activeTextFieldId = fieldId;
#ifdef XVT_MODERN
		XvtRenderFrontend_TextFieldPressed();
#endif
	}

#ifdef XVT_MODERN
	XvtRenderFrontend_TextEntry(fieldId);
#endif
	if (g_activeTextFieldId == fieldId) {
		uint8_t inputChar = Keyboard_PeekChar();
		if (ignoredChars != NULL) {
			int ignoredIndex;

			for (ignoredIndex = 0; ignoredChars[ignoredIndex] != '\0'; ++ignoredIndex) {
				if ((uint8_t)ignoredChars[ignoredIndex] == inputChar) {
					inputChar = 0;
					Keyboard_DiscardChar();
					break;
				}
			}
		}
		if (inputChar != 0) {
			int characterCode = inputChar;
			if (characterCode != 27)
				Keyboard_DiscardChar();
#ifdef XVT_MODERN
			if ((inputChar >= 32 && inputChar != 127) || inputChar == 32) {
#else
			if (isprint(characterCode) || inputChar == 32) {
#endif
				if (maxChars - 1 > g_textFieldLength) {
					text[g_textFieldLength++] = (char)inputChar;
					++g_textFieldCursorCharIndex;
					text[g_textFieldLength] = '\0';
				}
			} else if (inputChar == 13 || inputChar == 9) {
				completed = 1;
			} else if (inputChar == 8 && g_textFieldLength != 0) {
				--g_textFieldLength;
				--g_textFieldCursorCharIndex;
				text[g_textFieldLength] = '\0';
			}
		}
	}

	textWidth = FrontendText_MeasureWidth(text, fontSize);
	rectWidth = rect->right - rect->left;
	mouseX = 0;
	if (rectWidth + 1 < textWidth) {
		mouseX = rectWidth;
		mouseX -= fontSize;
		mouseX -= textWidth;
		++mouseX;
	}
	FrontendDisplay_GetScreenClipRect(&previousClipRect);
	FrontendDraw_RectAssign(&textClipRect, rect->left + 2, rect->top + 2, rect->right, rect->bottom);
	FrontendDisplay_SetScreenClipRect640x480(&textClipRect);
	FrontendText_Draw(fontSize, text, mouseX + rect->left + 2, rect->top + 2, g_textFieldColor);
	FrontendDisplay_SetScreenClipRect640x480(&previousClipRect);
	if (g_activeTextFieldId == fieldId) {
		int caretWidth;
		char savedChar;

		savedChar = text[g_textFieldCursorCharIndex];
		text[g_textFieldCursorCharIndex] = '\0';
		caretWidth = FrontendText_MeasureWidth(text, fontSize);
		text[g_textFieldCursorCharIndex] = savedChar;
		if (FrontendDisplay_GetFrameCounter() % 10 < 5) {
			FrontendDraw_RectCopy(&previousClipRect, rect);
			previousClipRect.left += mouseX + caretWidth + 1;
			previousClipRect.right = previousClipRect.left + 1;
			previousClipRect.top++;
			previousClipRect.bottom = previousClipRect.top + fontSize;
			FrontendDraw_Rect(&previousClipRect, 0, 0, g_textFieldColor, 1);
		}
	}

	return completed;
}

// FUNCTION: XVT 0x4DADA0
int FrontendText_LoadFont(int pointSize) {
	char scratchBuffer[1024];
	BitmapFont* font;
	XvtFile* stream;
	int slotIndex;
#ifndef XVT_MODERN
	void* fontHandle;
	void* dc;
	void* previousObject;
	int backBufferWasLocked;
	int blobCapacity;
	int blobUsed;
	int glyphIndex;
	unsigned int rowIndex;
	int rowSize;
	uint8_t* writeCursor;
	SIZE glyphExtent;
	RECT surfaceRect;
	RECT textRect;
	char glyphText[2];
	HRESULT result;
#endif

	if (pointSize > UINT8_MAX)
		pointSize = UINT8_MAX;
	else if (pointSize <= 0)
		pointSize = 1;

	font = NULL;
#ifndef XVT_MODERN
	blobCapacity = 0;
#endif
	if (g_frontState.fontBySize[pointSize] != NULL)
		return 1;

	for (slotIndex = 0; slotIndex < 10; ++slotIndex) {
		if (g_frontState.fontSlots[slotIndex].inUse == 0) {
			font = &g_frontState.fontSlots[slotIndex];
			break;
		}
	}
	if (font == NULL)
		return 0;

	sprintf(scratchBuffer, "times%u.abp", pointSize);
	stream = File_Open(scratchBuffer, g_fileModeReadBinary);
	if (stream != NULL) {
		File_Close(stream);
		if (FrontendText_LoadFontAtlasFile(scratchBuffer, slotIndex) != 0)
			return 1;
	}

#ifdef XVT_MODERN
	return 0;
#else
	fontHandle = CreateFontA(-pointSize, 0, 0, 0, 400, 0, 0, 0, 0, 4, 0, 3, 0x12, "times new roman");
	if (fontHandle == NULL)
		return 0;

	backBufferWasLocked = g_frontState.backBufferLocked;
	FrontendDisplay_UnlockBackBuffer();
	dc = GetDC(NULL);
	previousObject = SelectObject(dc, fontHandle);
	SetMapMode(dc, 1);
	SetTextCharacterExtra(dc, 0);
	font->glyphWidth[0] = 0;
	for (glyphIndex = 1; glyphIndex < 256; ++glyphIndex) {
		glyphText[0] = (char)glyphIndex;
		glyphText[1] = '\0';
		GetTextExtentPoint32A(dc, glyphText, 1, &glyphExtent);
		if (glyphExtent.cx < 0)
			glyphExtent.cx = 0;
		if (glyphExtent.cy < 0)
			glyphExtent.cy = 0;
		font->glyphWidth[glyphIndex] = (uint8_t)glyphExtent.cx;
		font->glyphHeight[glyphIndex] = (uint8_t)glyphExtent.cy;
		blobCapacity += glyphExtent.cy * glyphExtent.cx;
	}
	font->glyphHeight[0] = font->glyphHeight[1];
	SelectObject(dc, previousObject);
	ReleaseDC(NULL, dc);

	writeCursor = malloc(blobCapacity);
	if (writeCursor == NULL) {
		DeleteObject(fontHandle);
		if (backBufferWasLocked != 0)
			g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		return 0;
	}
	font->pGlyphBits = writeCursor;
	font->glyphBitOffset[0] = 0;
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = 640;
	surfaceRect.bottom = 480;

	for (;;) {
		result = g_frontState.backBufferSurface->lpVtbl->BltFast(
			g_frontState.backBufferSurface, 0, 0, g_frontState.offscreenSurface, &surfaceRect, 0);
		if (result == DX_DD_OK)
			break;
		if (result == DX_DDERR_SURFACELOST) {
			if (FrontendDisplay_RestoreLostSurfaces() != DX_DD_OK) {
				DeleteObject(fontHandle);
				if (backBufferWasLocked != 0)
					g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
				free(writeCursor);
				return 0;
			}
		} else if (result != DX_DDERR_WASSTILLDRAWING) {
			free(writeCursor);
			DeleteObject(fontHandle);
			if (backBufferWasLocked != 0)
				g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
			return 0;
		}
	}

	blobUsed = 0;
	glyphIndex = 1;
	for (;;) {
		result = ((FrontendTextSurfaceGetDcFunc)g_frontState.offscreenSurface->lpVtbl->GetDC)(
			g_frontState.offscreenSurface, &dc);
		if (result != DX_DD_OK) {
			if (result == DX_DDERR_SURFACELOST) {
				result = FrontendDisplay_RestoreLostSurfaces();
				if (result != DX_DD_OK)
					break;
			}
			if (result == DX_DDERR_WASSTILLDRAWING)
				continue;
			if (result != DX_DD_OK)
				break;
		}

		SetMapMode(dc, 1);
		previousObject = SelectObject(dc, fontHandle);
		SetTextColor(dc, 0xFFFFFF);
		SetBkColor(dc, 0);
		SetBkMode(dc, 2);
		SetRect(&textRect, 0, 0, 640, 480);
		glyphText[0] = (char)glyphIndex;
		glyphText[1] = '\0';
		ExtTextOutA(dc, 0, 0, 2, &textRect, glyphText, 1, NULL);
		SelectObject(dc, previousObject);
		((FrontendTextSurfaceReleaseDcFunc)g_frontState.offscreenSurface->lpVtbl->ReleaseDC)(
			g_frontState.offscreenSurface, dc);
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
		font->glyphBitOffset[glyphIndex] = blobUsed;

		for (rowIndex = 0; rowIndex < font->glyphHeight[glyphIndex]; ++rowIndex) {
			switch (g_frontState.displayBpp) {
				case 8:
					rowSize = FrontImage_EncodeGlyphRow(&g_frontState.rleRowBuffer, g_drawSurfacePtr,
														font->glyphWidth[glyphIndex]);
					break;
				case 16: {
					int columnIndex;

					for (columnIndex = 0; columnIndex < font->glyphWidth[glyphIndex]; ++columnIndex) {
						if (((const uint16_t*)g_drawSurfacePtr)[columnIndex] != 0)
							scratchBuffer[columnIndex] = 0xFF;
						else
							scratchBuffer[columnIndex] = 0;
					}
					rowSize =
						FrontImage_EncodeGlyphRow(&g_frontState.rleRowBuffer, (const uint8_t*)scratchBuffer,
												  font->glyphWidth[glyphIndex]);
					break;
				}
				default:
					break;
			}

			if (blobCapacity <= blobUsed + rowSize) {
				blobCapacity = 3 * blobCapacity / 2;
				font->pGlyphBits = realloc(font->pGlyphBits, blobCapacity);
				if (font->pGlyphBits == NULL)
					break;
				writeCursor = font->pGlyphBits + blobUsed;
			}
			if (font->pGlyphBits == NULL)
				break;
			memcpy(writeCursor, &g_frontState.rleRowBuffer, rowSize);
			blobUsed += rowSize;
			writeCursor += rowSize;
			g_drawSurfacePtr += g_frontState.drawSurfacePitch;
		}

		FrontendDisplay_UnlockBackBuffer();
		if (font->pGlyphBits == NULL)
			break;
		++glyphIndex;
		if (glyphIndex >= 256)
			break;
	}

	for (;;) {
		result = g_frontState.offscreenSurface->lpVtbl->BltFast(
			g_frontState.offscreenSurface, 0, 0, g_frontState.backBufferSurface, &surfaceRect, 0);
		if (result == DX_DD_OK)
			break;
		if ((result == DX_DDERR_SURFACELOST &&
			 (result = FrontendDisplay_RestoreLostSurfaces()) != DX_DD_OK) ||
			result != DX_DDERR_WASSTILLDRAWING) {
			glyphIndex = 0;
			break;
		}
	}

	if (backBufferWasLocked != 0)
		g_drawSurfacePtr = FrontendDisplay_LockBackBuffer();
	DeleteObject(fontHandle);
	if (blobCapacity > blobUsed) {
		font->pGlyphBits = realloc(font->pGlyphBits, blobUsed);
		if (font->pGlyphBits == NULL)
			glyphIndex = 0;
	}
	if (glyphIndex == 256) {
		font->pointSize = pointSize;
		font->inUse = 1;
		font->charSpacing = 0;
		font->field_60A = 1;
		g_frontState.fontBySize[pointSize] = font;
		sprintf(scratchBuffer, "times%u.abp", pointSize);
		FrontendText_SaveFontAtlasFile(scratchBuffer, (void**)font, blobUsed);
		return 1;
	}
	if (font->pGlyphBits != NULL)
		free(font->pGlyphBits);
	return 0;
#endif
}

// FUNCTION: XVT 0x4DB420
void FrontendText_FreeAllFonts(void) {
	int index;

	for (index = 0; index < 10; ++index) {
		if (g_frontState.fontSlots[index].inUse == 1) {
			if (g_frontState.fontSlots[index].pGlyphBits != 0) {
				free(g_frontState.fontSlots[index].pGlyphBits);
				g_frontState.fontSlots[index].pGlyphBits = 0;
			}
#ifdef XVT_MODERN
			XvtRenderAssets_FreeImage(&g_frontState.fontSlots[index]);
#endif
			g_frontState.fontSlots[index].inUse = 0;
		}
	}
	memset(g_frontState.fontBySize, 0, sizeof(g_frontState.fontBySize));
}

// FUNCTION: XVT 0x4DB470
void FrontendText_FreeFont(unsigned int pointSize) {
	int index;

	for (index = 0; index < 10; ++index) {
		if (g_frontState.fontSlots[index].inUse == 1 &&
			g_frontState.fontSlots[index].pointSize == (uint8_t)pointSize) {
			free(g_frontState.fontSlots[index].pGlyphBits);
			g_frontState.fontSlots[index].pGlyphBits = 0;
#ifdef XVT_MODERN
			XvtRenderAssets_FreeImage(&g_frontState.fontSlots[index]);
#endif
			g_frontState.fontSlots[index].inUse = 0;
			g_frontState.fontBySize[pointSize] = 0;
			return;
		}
	}
}

// FUNCTION: XVT 0x4DB4E0
int FrontendText_Draw(int fontSize, const char* str, int x, int y, int color) {
	BitmapFont* font;
	int currentColor;
	int textIndex;
	int result;
	ImageResource glyph;

	if (str == NULL)
		return 0;
	if (fontSize < 0 || fontSize > 255)
		return 0;
	font = g_frontState.fontBySize[fontSize];
	if (font == NULL)
		return 0;

	currentColor = color;
	textIndex = 0;
	result = 0;
	if (*str != '\0') {
		do {
			uint8_t character;

			if (x >= 640)
				break;
			character = (uint8_t)str[textIndex];
			if (character == 1) {
				currentColor = color;
			} else if (character >= 2 && character <= 6) {
				currentColor = g_frontState.textColorCodes[character - 1];
			} else {
				glyph.width = font->glyphWidth[character];
				glyph.height = font->glyphHeight[character];
				glyph.pixelCount = 0;
				glyph.isCompressed = 1;
				glyph.pixels = &font->pGlyphBits[font->glyphBitOffset[character]];
				result |= FrontImage_DrawGlyph(&glyph, x, y, currentColor, 1);
				x += font->charSpacing + font->glyphWidth[(uint8_t)str[textIndex]];
			}
			++textIndex;
		} while (str[textIndex] != '\0');
	}
	return result;
}

// FUNCTION: XVT 0x4DB640
int FrontendText_DrawCentered(int fontSize, const char* str, RECT* rect, int color) {
	BitmapFont* font;
	int currentColor;
	int x;
	int textIndex;
	int y;
	int result;
	ImageResource glyph;

	if (str == NULL)
		return 0;
	if (fontSize < 0 || fontSize > 255)
		return 0;
	font = g_frontState.fontBySize[fontSize];
	if (font == NULL)
		return 0;

	currentColor = color;
	x = rect->left + ((rect->right - rect->left) >> 1) - (FrontendText_MeasureWidth(str, fontSize) >> 1);
	textIndex = 0;
	y = rect->top + ((rect->bottom - rect->top) >> 1);
	result = 0;
	y -= FrontendText_GetFontHeight(fontSize) >> 1;
	if (*str != '\0') {
		do {
			uint8_t character;

			if (x >= 640)
				break;
			character = (uint8_t)str[textIndex];
			if (character == 1) {
				currentColor = color;
			} else if (character >= 2 && character <= 6) {
				currentColor = g_frontState.textColorCodes[character - 1];
			} else {
				glyph.width = font->glyphWidth[character];
				glyph.height = font->glyphHeight[character];
				glyph.pixelCount = 0;
				glyph.isCompressed = 1;
				glyph.pixels = &font->pGlyphBits[font->glyphBitOffset[character]];
				result |= FrontImage_DrawGlyph(&glyph, x, y, currentColor, 1);
				x += font->charSpacing + font->glyphWidth[(uint8_t)str[textIndex]];
			}
			++textIndex;
		} while (str[textIndex] != '\0');
	}
	return result;
}

// FUNCTION: XVT 0x4DB7D0
int FrontendText_DrawAlignedInRect(int fontSize, const char* str, RECT* rect, int centerH, int centerV,
								   int color) {
	int currentColor;
	int halfWidth;
	int x;
	int textIndex;
	BitmapFont* font;
	int y;
	int result;
	ImageResource glyph;

	if (str == NULL)
		return 0;
	if (fontSize < 0 || fontSize > 255)
		return 0;
	font = g_frontState.fontBySize[fontSize];
	if (font == NULL)
		return 0;

	currentColor = color;
	halfWidth = FrontendText_MeasureWidth(str, fontSize) >> 1;
	if (centerH) {
		x = rect->left + ((rect->right - rect->left) >> 1) - halfWidth;
	} else {
		x = rect->left;
	}
	if (centerV) {
		y = rect->top + ((rect->bottom - rect->top) >> 1);
		y -= FrontendText_GetFontHeight(fontSize) >> 1;
	} else {
		y = rect->top;
	}

	textIndex = 0;
	result = 0;
	if (*str != '\0') {
		do {
			uint8_t character;

			if (x >= 640)
				break;
			character = (uint8_t)str[textIndex];
			if (character == 1) {
				currentColor = color;
			} else if (character >= 2 && character <= 6) {
				currentColor = g_frontState.textColorCodes[character - 1];
			} else {
				glyph.width = font->glyphWidth[character];
				glyph.height = font->glyphHeight[character];
				glyph.pixelCount = 0;
				glyph.isCompressed = 1;
				glyph.pixels = &font->pGlyphBits[font->glyphBitOffset[character]];
				result |= FrontImage_DrawGlyph(&glyph, x, y, currentColor, 1);
				x += font->charSpacing + font->glyphWidth[(uint8_t)str[textIndex]];
			}
			++textIndex;
		} while (str[textIndex] != '\0');
	}
	return result;
}

// FUNCTION: XVT 0x4DB980
int FrontendText_DrawLineArrayInRect(int fontSize, const char** lines, int lineCount, const RECT* rect,
									 int color, int centerHorizontally, int centerVertically,
									 int lineSpacing) {
	const char** lineCursor;
	int drawY;
	int currentColor;
	int drawStatus;
	int linesRemaining;
	int halfLineWidth;
	int drawX;
	BitmapFont* font;
	int charIndex;
	const char* charPtr;
	int fontHeight;
	ImageResource glyph;

	lineCursor = lines;
	if (lineCursor == NULL)
		return 0;
	if (fontSize < 0 || fontSize > 255)
		return 0;
	font = g_frontState.fontBySize[fontSize];
	if (font == NULL)
		return 0;

	drawStatus = 0;
	currentColor = color;
	if (centerVertically) {
		drawY = rect->top + ((rect->bottom - rect->top) >> 1);
		fontHeight = FrontendText_GetFontHeight(fontSize);
		drawY -= (lineSpacing * (lineCount - 1) + lineCount * fontHeight) >> 1;
	} else {
		drawY = rect->top;
	}
	if (lineCount > 0) {
		linesRemaining = lineCount;
		do {
			halfLineWidth = FrontendText_MeasureWidth(*lineCursor, fontSize) >> 1;
			if (centerHorizontally) {
				drawX = rect->left + ((rect->right - rect->left) >> 1) - halfLineWidth;
			} else {
				drawX = rect->left;
			}
			charIndex = 0;
			if (**lineCursor != '\0') {
				do {
					uint8_t character;

					if (drawX >= 640)
						break;
					charPtr = &(*lineCursor)[charIndex];
					character = (uint8_t)*charPtr;
					if (character == 1) {
						currentColor = color;
					} else if (character >= 2 && character <= 7) {
						currentColor = g_frontState.textColorCodes[character - 1];
					} else {
						glyph.width = font->glyphWidth[character];
						glyph.height = font->glyphHeight[(uint8_t)*charPtr];
						glyph.pixelCount = 0;
						glyph.isCompressed = 1;
						glyph.pixels = &font->pGlyphBits[font->glyphBitOffset[(uint8_t)*charPtr]];
						drawStatus |= FrontImage_DrawGlyph(&glyph, drawX, drawY, currentColor, 1);
						drawX += font->charSpacing + font->glyphWidth[(uint8_t)(*lineCursor)[charIndex]];
					}
					++charIndex;
				} while ((*lineCursor)[charIndex] != '\0');
			}
			if (drawX >= 640)
				drawStatus |= 4;
			++lineCursor;
			drawY += FrontendText_GetFontHeight(fontSize) + lineSpacing;
			--linesRemaining;
		} while (linesRemaining != 0);
	}

	return drawStatus;
}

// FUNCTION: XVT 0x4DBBD0
int FrontendText_DrawWrapped(int fontSize, const char* str, RECT* rect, int color, int lineSpacing,
							 int firstVisibleLine) {
	BitmapFont* font;
	int x;
	int lineIndex;
	char word[256];
	ImageResource glyph;
	RECT savedClip;
	int index;
	int y;
	int wordLength;
	int atLineStart;
	unsigned int currentColor;
	unsigned int nextColor;
	uint8_t currentChar;
	uint8_t glyphChar;

	if (str == NULL) {
		return 0;
	}
	if (*str == '\0') {
		return 0;
	}
	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}
	font = g_frontState.fontBySize[fontSize];
	if (font == NULL) {
		return 0;
	}

	index = 0;
	x = rect->left;
	lineIndex = 0;
	wordLength = 0;
	atLineStart = 1;
	currentColor = (unsigned int)color;
	y = rect->top;
	FrontendDisplay_GetScreenClipRect(&savedClip);
	FrontendDisplay_SetScreenClipRect640x480(rect);

	do {
		nextColor = currentColor;
		currentChar = (uint8_t)str[index];
		if (currentChar == 1) {
			nextColor = (unsigned int)color;
		} else if (currentChar >= 2 && currentChar <= 7) {
			nextColor = (unsigned int)g_frontState.textColorCodes[currentChar - 1];
		} else if (currentChar != ' ' && currentChar != '\n' && str[index + 1] != '\0' &&
				   currentChar != '$') {
			atLineStart = 0;
			word[wordLength++] = (char)currentChar;
		} else {
			int i;
			int* charIndex;

			charIndex = &i;
			if (atLineStart == 1 && currentChar == ' ') {
				++index;
				continue;
			}

			if (currentChar != '\n' && currentChar != '$') {
				word[wordLength++] = (char)currentChar;
			}
			word[wordLength] = '\0';
			if (x + FrontendText_MeasureWidth(word, fontSize) > rect->right - fontSize) {
				if (lineIndex >= firstVisibleLine) {
					y += fontSize + lineSpacing;
				}
				++lineIndex;
				atLineStart = 1;
				x = rect->left;
			}

			for (*charIndex = 0; *charIndex < wordLength; ++*charIndex) {
				if (lineIndex < firstVisibleLine) {
					x += font->glyphWidth[(uint8_t)word[*charIndex]] + font->charSpacing;
					if (x <= rect->right) {
						continue;
					}
				} else {
					glyphChar = (uint8_t)word[*charIndex];
					glyph.width = font->glyphWidth[glyphChar];
					glyph.height = font->glyphHeight[glyphChar];
					glyph.pixelCount = 0;
					glyph.isCompressed = 1;
					glyph.pixels = &font->pGlyphBits[font->glyphBitOffset[glyphChar]];
					FrontImage_DrawGlyph(&glyph, x, y, currentColor, 1);
					x += font->glyphWidth[glyphChar] + font->charSpacing;
					if (x <= rect->right - fontSize) {
						continue;
					}
				}

				if (lineIndex >= firstVisibleLine) {
					y += fontSize + lineSpacing;
				}
				++lineIndex;
				x = rect->left;
			}

			if (currentChar == '\n' || currentChar == '$') {
				if (lineIndex >= firstVisibleLine) {
					y += fontSize + lineSpacing;
				}
				++lineIndex;
				atLineStart = 1;
				x = rect->left;
			}
			wordLength = 0;
		}

		currentColor = nextColor;
		++index;
	} while (str[index] != '\0');

	FrontendDisplay_SetScreenClipRect640x480(&savedClip);
	return lineIndex;
}

// FUNCTION: XVT 0x4DBF70
int FrontendText_GetFontHeight(int fontSize) {
	BitmapFont* font;

	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}
	font = g_frontState.fontBySize[fontSize];
	if (font == 0) {
		return 0;
	}
	return font->glyphHeight[0];
}

// FUNCTION: XVT 0x4DBFA0
int FrontendText_MeasureWidth(const char* str, int fontSize) {
	BitmapFont* font;
	int width;
	int index;

	if (str == 0) {
		return 0;
	}
	if (fontSize < 0 || fontSize > 255) {
		return 0;
	}

	font = g_frontState.fontBySize[fontSize];
	if (font == 0) {
		return 0;
	}

	width = 0;
	index = 0;
	if (*str != '\0') {
		do {
			if ((uint8_t)str[index] > 6u) {
				width += font->glyphWidth[(uint8_t)str[index]];
				width += font->charSpacing;
			}
			++index;
		} while (str[index] != '\0');
	}

	return width - font->charSpacing;
}

// FUNCTION: XVT 0x4DC020
void FrontendText_SaveFontAtlasFile(char* fileName, void** font, unsigned int glyphBlobSize) {
	uint8_t diskHeader[0x60B];
	XvtFile* stream;

	if (font != NULL) {
		stream = File_Open(fileName, "wb");
		if (stream != NULL) {
			memcpy(diskHeader, font, sizeof(diskHeader));
			*(unsigned int*)diskHeader = glyphBlobSize;
			File_WriteCount(stream, diskHeader, sizeof(diskHeader));
			File_WriteCount(stream, *font, glyphBlobSize);
			File_Close(stream);
		}
	}
}

// FUNCTION: XVT 0x4DC0A0
int FrontendText_LoadFontAtlasFile(const char* fileName, int slotIndex) {
	BitmapFont* font;
	XvtFile* stream;
	size_t glyphBlobSize;
	void* glyphBits;
#ifdef XVT_MODERN
	uint8_t diskHeader[0x60B];
	uint32_t diskGlyphBlobSize;
#endif

	font = &g_frontState.fontSlots[slotIndex];
	stream = File_Open(fileName, "rb");
	if (stream == NULL) {
		return 0;
	}

#ifdef XVT_MODERN
	if (!File_ReadCount(stream, diskHeader, sizeof(diskHeader))) {
		File_Close(stream);
		return 0;
	}
	memcpy(&diskGlyphBlobSize, &diskHeader[0], sizeof(diskGlyphBlobSize));
	memcpy(font->glyphBitOffset, &diskHeader[4], sizeof(font->glyphBitOffset));
	memcpy(font->glyphHeight, &diskHeader[0x404], sizeof(font->glyphHeight));
	memcpy(font->glyphWidth, &diskHeader[0x504], sizeof(font->glyphWidth));
	memcpy(&font->pointSize, &diskHeader[0x604], sizeof(font->pointSize));
	font->inUse = diskHeader[0x608];
	font->charSpacing = diskHeader[0x609];
	font->field_60A = diskHeader[0x60A];
	glyphBlobSize = diskGlyphBlobSize;
#else
	File_ReadCount(stream, font, 0x60B);
	glyphBlobSize = *(const uint32_t*)(const void*)font;
#endif
	glyphBits = malloc(glyphBlobSize);
	font->pGlyphBits = glyphBits;
	if (glyphBits == NULL) {
		font->inUse = 0;
		File_Close(stream);
		return 0;
	}

	File_ReadCount(stream, glyphBits, glyphBlobSize);
	g_frontState.fontBySize[font->pointSize] = font;
	File_Close(stream);
#ifdef XVT_MODERN
	XvtRenderAssets_RegisterImage(font, 0, fileName, XVT_IMAGE_ABP, 0, 256, (uint16_t)font->pointSize, 0, 0);
	XvtRenderFrontend_FontLoaded(font);
#endif
	return 1;
}

// FUNCTION: XVT 0x4DC140
int FrontendText_ResetGlyphScratchBuffer(int frames) {
	memset(g_frontState.glyphScratchBuffer, 0, sizeof(g_frontState.glyphScratchBuffer));
	g_frontState.glyphScratchTtl = frames;
	g_frontState.glyphScratchReload = frames;
	return 1;
}

// FUNCTION: XVT 0x4DC170
int FrontendText_ResetGlyphScratch(void) {
	g_frontState.glyphScratchTtl = 0;
	g_frontState.glyphScratchReload = 0;
	return 1;
}

// FUNCTION: XVT 0x4DC190
int FrontendText_PushGlyphScratchTtl(void) {
	g_savedGlyphScratchTtl = g_frontState.glyphScratchTtl;
	g_frontState.glyphScratchTtl = 0;
	return 1;
}

// FUNCTION: XVT 0x4DC1B0
int FrontendText_PopGlyphScratchTtl(void) {
	g_frontState.glyphScratchTtl = g_savedGlyphScratchTtl;
	return 1;
}

// FUNCTION: XVT 0x4F8620
void FrontendText_DrawFormattedWrappedText(RECT* rect, const uint8_t* text, int suppressCenteredHeadings) {
	RECT drawRect;
	char lineBuffer[320];
	int16_t lineStart;
	int16_t scanIndex;
	int16_t segmentStart;
	int16_t rectWidth;
	int16_t lineEnd;
	int16_t lastSpace;
	int16_t done;
	int savedScanIndex;
	const uint8_t* currentPtr;
	uint8_t currentChar;
	int16_t colorActive;
	int16_t sourceIndex;
	int16_t outputIndex;

	FrontendDraw_RectCopy(&drawRect, rect);
	lineStart = -1;
	scanIndex = 0;
	segmentStart = 0;
	drawRect.bottom = drawRect.top + 13;
	lineBuffer[0] = '\0';
	rectWidth = (int16_t)drawRect.right - (int16_t)drawRect.left;
	lineEnd = -1;
	lastSpace = 0;
	done = 0;
	do {
		savedScanIndex = scanIndex;
		currentPtr = &text[scanIndex];
		currentChar = *currentPtr;
		if (*currentPtr == '$' || currentChar == '\0') {
			lineStart = segmentStart;
			if ((int16_t)suppressCenteredHeadings != 0 || currentChar != '$')
				lineEnd = scanIndex;
			else
				lineEnd = scanIndex - 1;
			segmentStart = ++scanIndex;
			if (text[scanIndex] == '\0')
				done = 1;
		} else {
			if (currentChar == ' ')
				lastSpace = scanIndex;
			if (isspace(currentChar)) {
				++scanIndex;
				lineBuffer[savedScanIndex - segmentStart] = (char)*currentPtr;
			} else {
				while (!isspace(text[scanIndex]) && text[scanIndex] != '$' && text[scanIndex] != '\0') {
					if (text[scanIndex] == '[')
						lineBuffer[scanIndex - segmentStart] = 2;
					else if (text[scanIndex] == ']')
						lineBuffer[scanIndex - segmentStart] = 1;
					else
						lineBuffer[scanIndex - segmentStart] = (char)text[scanIndex];
					++scanIndex;
				}
			}
			lineBuffer[scanIndex - segmentStart] = '\0';
			if (rectWidth <= (int16_t)FrontendText_MeasureWidth(lineBuffer, 10)) {
				lineStart = segmentStart;
				segmentStart = lastSpace + 1;
				scanIndex = lastSpace + 1;
				lineEnd = lastSpace;
			}
		}

		colorActive = 0;
		for (sourceIndex = 0; lineStart > sourceIndex; ++sourceIndex) {
			if (text[sourceIndex] == '[')
				colorActive = 1;
			if (text[sourceIndex] == ']')
				colorActive = 0;
		}
		if (lineStart != -1) {
			outputIndex = 0;
			if (colorActive)
				lineBuffer[outputIndex++] = 2;
			lineBuffer[outputIndex] = '\0';
			while (lineEnd >= lineStart) {
				currentChar = text[lineStart];
				if (currentChar == '[')
					lineBuffer[outputIndex] = 2;
				else if (currentChar == ']')
					lineBuffer[outputIndex] = 1;
				else
					lineBuffer[outputIndex] = (char)currentChar;
				++outputIndex;
				++lineStart;
			}
			lineBuffer[outputIndex] = '\0';
			if ((int16_t)suppressCenteredHeadings != 0 || lineBuffer[0] != '>') {
				FrontendText_DrawAlignedInRect(10, lineBuffer, &drawRect, 0, 1, 0xFFFF);
			} else {
				FrontendDraw_RectOffsetXY(&drawRect, 0, 1);
				FrontendText_DrawCentered(10, &lineBuffer[1], &drawRect, g_colorLightBlue);
				FrontendDraw_RectOffsetXY(&drawRect, 0, -1);
			}
			lineStart = -1;
			lineEnd = -1;
			FrontendDraw_RectOffsetXY(&drawRect, 0, 14);
		}
	} while (!done);
}
