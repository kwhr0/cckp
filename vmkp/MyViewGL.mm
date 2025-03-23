#import "MyDocument.h"

@interface MyViewGL : NSOpenGLView
@end

@implementation MyViewGL

- (void)awakeFromNib {
	[[self window] makeFirstResponder:self];
}

- (MyDocument *)document {
	return [[NSDocumentController sharedDocumentController] documentForWindow:[self window]];
}

- (void)keyDown:(NSEvent *)event {
	[[self document] key:[event keyCode] isUp:FALSE];
}

- (void)keyUp:(NSEvent *)event {
	[[self document] key:[event keyCode] isUp:TRUE];
}

- (void)drawRect:(NSRect)rect {
	uint32_t *buf = new uint32_t[TEX_X * TEX_Y], *dp = buf;
	uint16_t *sp = (uint16_t *)self.document.vram;
	for (int i = 0; i < TEX_X * TEX_Y; i++) {
		int d = *sp++, r = d >> 11 & 0x1f, g = d >> 5 & 0x3f, b = d & 0x1f;
		r = r << 3 | r >> 2;
		g = g << 2 | g >> 4;
		b = b << 3 | b >> 2;
		*dp++ = r | g << 8 | b << 16;
	}
	glEnable(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_X, TEX_Y, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0); glVertex2f(-1, 1);
	glTexCoord2f(0, 1); glVertex2f(-1, -1);
	glTexCoord2f(1, 1); glVertex2f(1, -1);
	glTexCoord2f(1, 0); glVertex2f(1, 1);
	glEnd();
	glFlush();
	delete[] buf;
}

@end
