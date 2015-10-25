/* Copyright 2011 Pyarelal Knowles, under GNU LGPL (see LICENCE.txt) */

#include "prec.h"

#include <set>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "quickgui.h"
#include "texture.h"
#include "matrix.h"
#include "includegl.h"
#include "util.h"
#include "config.h"

namespace QG
{

bool compareDepth(Widget* a, Widget* b)
{
	return a->Z < b->Z;
}

Widget* Widget::focus = NULL;
NineBoxPool Widget::nineboxPool;
WidgetSet* Widget::instances = NULL;
WidgetSet* Widget::roots = NULL;
Immediate Widget::mygl;

float round(float d)
{
  return floor(d + 0.5f);
}

void WidgetCallbackData::operator()()
{
	if (arg)
	{
		if (w)
			ptrwa(w, arg);
		else
			ptra(arg);
	}
	else
	{
		if (w)
			ptrw(w);
		else
			ptrv();
	}
}

Widget::Widget() : width(size.x), height(size.y)
{
	Z = 0.0f;
	margin = 0;
	border = 0;
	padding = 0;
	size = vec2i(100, 10);
	stack = VERTICAL;
	fill = BOTH;
	expand = NONE;
	anchor = TOP_LEFT;
	autoDelete = false;
	subImage = 0;
	nineboxImage = -1;
	dirty = true;
	parent = NULL;
	root = this;
	mouseOver = false;
	offset = vec2i(0);
	position = vec2i(0);
	hidden = false;
	
	if (!instances)
		instances = new WidgetSet();
	if (!roots)
		roots = new WidgetSet();
	instances->insert(this);
	roots->insert(this);
}
Widget::~Widget()
{
	if (nineboxImage >= 0)
	{
		nineboxPool.remove(nineboxImage);
	}

	while (children.size())
	{
		//delete child OR fix child's parent pointer, add children as roots etc
		//depending on the autoDelete flag
		rem(*children.begin());
	}
	
	instances->erase(this);
	roots->erase(this);
	if (roots->size() == 0)
	{
		delete roots;
		roots = NULL;
	}
	if (instances->size() == 0)
	{
		delete instances;
		instances = NULL;
	}
}
void Widget::loadImages(std::string mapfile)
{
	//open text file
	ifstream ifile(mapfile.c_str());
	if (!ifile.is_open())
	{
		printf("Error: Could not open GUI config \"%s\"\n", mapfile.c_str());
		return;
	}
	
	//extract base path from the filename (paths will be relative)
	std::string base;
	int b = (int)mapfile.find_last_of("/");
	if (b > 0)
		base = mapfile.substr(0, b+1);
	
	//read mapping on each line
	int numLoaded = 0;
	int numTotal = 0;
	std::string line;
	while (getline(ifile, line))
	{
		std::string id, file;
		int borderSize, count;
		std::stringstream lineReader(line);
		
		//extract info
		lineReader >> id;
		if (id[0] == '#')
			continue; //ignore comments
		lineReader >> file >> borderSize >> count;
		if (lineReader.bad())
			break;
		
		//add to ninebox's texture atlas
		if (nineboxPool.load(id, base + file, borderSize, count))
			numLoaded += count;
		numTotal += count;
	}

	printf("Loaded %i/%i GUI icons\n", numLoaded, numTotal);
}
void Widget::releaseAll()
{
	nineboxPool.release();
}
void Widget::update()
{
}
void Widget::packed()
{
	if (nineboxImage >= 0)
	{
		NineBox& box = nineboxPool[nineboxImage];
		box.subIndex = subImage;
		box.border = border;
		box.size.x = padding.left + width + padding.right;
		box.size.y = padding.top + height + padding.bottom;
		box.pos = position - vec2i(padding.left, padding.top);
		box.Z = Z + 0.0f;
	}
}
vec2i Widget::placeWidthAnchor(vec2i bounds, vec2i size, Anchor a)
{
	switch (a)
	{
	case CENTER: return (bounds - size) / 2;
	case TOP: return vec2i((bounds.x - size.x) / 2, 0);
	case BOTTOM: return vec2i((bounds.x - size.x) / 2, bounds.y - size.y);
	case LEFT: return vec2i(0, (bounds.y - size.y) / 2);
	case RIGHT: return vec2i(bounds.x - size.x, (bounds.y - size.y) / 2);
	case TOP_LEFT: return vec2i(0, 0);
	case TOP_RIGHT: return vec2i(bounds.x - size.x, 0);
	case BOTTOM_LEFT: return vec2i(0, bounds.y - size.y);
	case BOTTOM_RIGHT: return vec2i(bounds.x - size.x, bounds.y - size.y);
	}
	return vec2i(0);
}
void Widget::pack()
{
	if (!children.size())
		return; //no need to pack

	//find x/y coordinates of content area
	vec2i start = getStart();
	
	//boinds keeps track of the space the children take up
	vec2i bounds(0);
	
	//count the number of objects that want to expand their given packing area
	int expandCount = 0;

	//begin by updating all children and calculating bounds
	vec2i curPos(0);
	for (WidgetList::iterator it = children.begin(); it != children.end(); ++it)
	{
		(*it)->root = root;
	
		if ((*it)->hidden)
			continue;
		
		//recursively pack children
		(*it)->pack(); //won't do anything to content/leaf widgets
		
		//update child to refresh its bounds
		(*it)->update();
		
		if (stack & HORIZONTAL && (*it)->expand & HORIZONTAL && (*it)->fill & HORIZONTAL)
			(*it)->width = 0;
		if (stack & VERTICAL && (*it)->expand & VERTICAL && (*it)->fill & VERTICAL)
			(*it)->height = 0;
		
		//get the bounds of the updated object
		vec2i childBounds = (*it)->getBounds();
		bounds.x = max(bounds.x, curPos.x + childBounds.x);
		bounds.y = max(bounds.y, curPos.y + childBounds.y);
		
		//count the number of children expanding
		if (stack & HORIZONTAL)
		{
			curPos.x += childBounds.x;
			if ((*it)->expand & HORIZONTAL)
				++expandCount;
		}
		if (stack & VERTICAL)
		{
			curPos.y += childBounds.y;
			if ((*it)->expand & VERTICAL)
				++expandCount;
		}
	}
	
	//widget dimension in stack direction only expanded as needed.
	//widget dimension not in stack direction always shrink to best fit.
	if (stack & HORIZONTAL && !(expand & VERTICAL) && fill & VERTICAL)
		height = 0;
	if (stack & VERTICAL && !(expand & HORIZONTAL) && fill & HORIZONTAL)
		width = 0;
	width = max(width, bounds.x);
	height = max(height, bounds.y);
	
	//calculate space left to expand expanding child widgets
	int expandPixels = 0;
	if (stack & VERTICAL)
		expandPixels = height - bounds.y;
	else if (stack & HORIZONTAL)
		expandPixels = width - bounds.x;
	int expandLeft = expandPixels;
	int expandStep = ceil(expandLeft, max(1, expandCount));
	
	//FIXME: an additional update is needed to account for a forced container enlargement (eg in DropFrame::update())
	update();
	
	//finally, position all children, taking from expandPixels as necessary
	curPos = vec2i(0);
	for (WidgetList::iterator it = children.begin(); it != children.end(); ++it)
	{
		if ((*it)->hidden)
			continue;
			
		vec2i childBounds = (*it)->getBounds();
		
		//calculate pack area, increasing if child expands
		vec2i packArea = childBounds;
		if (stack & VERTICAL)
		{
			packArea.x = width;
			if ((*it)->expand & VERTICAL)
			{
				packArea.y += min(expandLeft, expandStep);
				expandLeft -= min(expandLeft, expandStep);
			}
		}
		else if (stack & HORIZONTAL)
		{
			packArea.y = height;
			if ((*it)->expand & HORIZONTAL)
			{
				packArea.x += min(expandLeft, expandStep);
				expandLeft -= min(expandLeft, expandStep);
			}
		}

		//increase child dimensions if child fills
		if ((*it)->fill & VERTICAL && childBounds.y < packArea.y)
			(*it)->height += packArea.y - childBounds.y;
		if ((*it)->fill & HORIZONTAL && childBounds.x < packArea.x)
			(*it)->width += packArea.x - childBounds.x;

		childBounds = (*it)->getBounds(); //dimensions may have changed
		
		//place child within packArea according to child's anchor
		vec2i place = placeWidthAnchor(packArea, childBounds, (*it)->anchor);
		(*it)->offset = curPos + place;

		//advance to the next child
		if (stack & HORIZONTAL) curPos.x += packArea.x;
		if (stack & VERTICAL) curPos.y += packArea.y;
	}
}
void Widget::packPos()
{
	if (!hidden)
	{
		position = offset + getStart();
		if (parent)
		{
			position += parent->position;
			//printf("%i,%i %i,%i %i,%i\n", parent->position.x, parent->position.y, offset.x, offset.y, getStart().x, getStart().y);
		}
		
		for (WidgetList::iterator it = children.begin(); it != children.end(); ++it)
				(*it)->packPos();
	}
	
	packed();
}
void Widget::_add(Widget* w)
{
	if (!w)
	{
		printf("ERROR! Trying to add NULL widget\n");
		return;
	}	
	
	//can only attach roots to the tree
	assert(roots->find(w) != roots->end());
	roots->erase(w);
	
	children.push_back(w);
	
	w->parent = this;
	w->root = root;
	w->onAdd();
	w->Z = Z;
	w->setDirty();
	
	w->hide(hidden || w->hidden);
}
void Widget::_rem(Widget* w)
{
	//find the child
	WidgetList::iterator found;
	for (found = children.begin(); found != children.end(); ++found)
		if (*found == w)
			break;
			
	//must exist
	assert(found != children.end());
	
	//remove
	children.erase(found);
	
	//delete child if the autoDelete flag is set
	if (w->autoDelete)
		delete w;
	else
	{
		w->parent = NULL;
		w->root = w;
	}
	setDirty();
}
void Widget::add(Widget& w) {_add(&w);}
void Widget::add(Widget* w) {_add(w);}
void Widget::rem(Widget& w) {_rem(&w);}
void Widget::rem(Widget* w) {_rem(w);}
void Widget::setPosition(vec2i pos)
{
	offset = pos;
	setDirty();
}
void Widget::setDirty()
{
	root->dirty = true;
}
vec2i Widget::getStart()
{
	int w = margin.left + border.left + padding.left;
	int h = margin.top + border.top + padding.top;
	return vec2i(w, h);
}
vec2i Widget::getBounds()
{
	int w = margin.left + border.left + padding.left + width + padding.right + border.right + margin.right;
	int h = margin.top + border.top + padding.top + height + padding.bottom + border.bottom + margin.bottom;
	return vec2i(w, h);
}
void Widget::hide(bool enable)
{
	if (hidden == enable)
		return;

	hidden = enable;
	//TODO: implement proper hidden
	if (hidden)
		position.x = -99999; //hidden :D
	packed();
	setDirty();

	for (WidgetList::iterator it = children.begin(); it != children.end(); ++it)
		(*it)->hide(enable);
}
bool Widget::isHidden()
{
	return hidden;
}
void Widget::drag(vec2i pos) {}
void Widget::mouseDown() {}
void Widget::mouseUp() {}
void Widget::mouseEnter() {}
void Widget::mouseLeave() {}
void Widget::keyDown(unsigned char c) {}
void Widget::keyUp(unsigned char c) {}
void Widget::click()
{
	if (callbacks.find(CLICK) != callbacks.end())
		callbacks[CLICK]();
}
void Widget::clickChild(int i) {}
void Widget::onAdd() {}
void Widget::draw(mat44 modelview)
{
	if (roots)
	{
		for (WidgetSet::iterator it = roots->begin(); it != roots->end(); ++it)
		{
			if ((*it)->dirty)
			{
				//FIXME: DON'T DO TWO PASSES! Not sure how to do this yet, but for a quick fix this works in the current app
				(*it)->pack();
				(*it)->pack();
				(*it)->packPos();
				(*it)->dirty = false;
			}
		}
	}
	
	#if !QUICK_WIDGET_DEBUG
	nineboxPool.draw(modelview);
	#endif
	
	if (instances)
		for (WidgetSet::iterator it = instances->begin(); it != instances->end(); ++it)
			if (!(*it)->hidden)
				(*it)->drawContent(modelview * mat44::translate(vec3f((*it)->position, 0.0f)));
}
bool Widget::allMouseDown(vec2i pos)
{
	//FIXME: cache sorted roots
	if (!roots)
		return false;
	WidgetList sortedRoots(roots->begin(), roots->end());
	std::sort(sortedRoots.begin(), sortedRoots.end(), compareDepth);
	for (int i = (int)sortedRoots.size() - 1; i >= 0; --i)
	{
		if (sortedRoots[i]->mouseDown(pos))
			return true;
	}
	return false;
} 
void Widget::allMouseUp(vec2i pos)
{
	/*
	//FIXME: cache sorted roots
	if (!roots)
		return;
	WidgetList sortedRoots(roots->begin(), roots->end());
	std::sort(sortedRoots.begin(), sortedRoots.end(), compareDepth);
	for (int i = (int)sortedRoots.size() - 1; i >= 0; --i)
		sortedRoots[i]->mouseUp(pos);
	*/
	
	Widget* child = NULL;
	Widget* tmp = Widget::focus;
	Widget::focus = NULL;
	while (tmp)
	{
		if (child)
		{
			for (int i = 0; i < (int)tmp->children.size(); ++i)
				if (tmp->children[i] == child)
					tmp->clickChild(i);
		}
			
		vec2i wpos = pos - (tmp->position - tmp->getStart());
		tmp->mouseUp();
		tmp->mouseUp(wpos);
		
		child = tmp;
		tmp = tmp->parent;
	}
}
bool Widget::allMouseMove(vec2i pos)
{
	//FIXME: cache sorted roots
	if (!roots)
		return false;
	WidgetList sortedRoots(roots->begin(), roots->end());
	std::sort(sortedRoots.begin(), sortedRoots.end(), compareDepth);
	for (int i = (int)sortedRoots.size() - 1; i >= 0; --i)
		if (sortedRoots[i]->mouseMove(pos))
			return true;
	return false;
}
bool Widget::mouseDown(vec2i pos)
{
	if (hidden)
		return false;
	
	//assert(!Widget::focus);
			
	//is the mouse inside the widget?
	vec2i subPos = pos - (position - getStart());
	vec2i bounds = getBounds();
	if (subPos.x >= 0 && subPos.y >= 0 && subPos.x < bounds.x && subPos.y < bounds.y)
	{
		mouseDown();
		drag(subPos);
			
		if (!children.size())
		{
			Widget::focus = this;
			return true;
		}
			
		for (WidgetList::iterator it = children.begin(); it != children.end(); ++it)
			if ((*it)->mouseDown(pos))
				return true;
	}
	return false;
}
void Widget::mouseUp(vec2i pos)
{
	vec2i bounds = getBounds();
	if (pos.x >= 0 && pos.y >= 0 && pos.x < bounds.x && pos.y < bounds.y)
		click();
}
bool Widget::mouseMove(vec2i pos)
{
	//ignore hidden
	if (hidden)
		return false;
	
	//if this is say, a button or slider in focus, we have just performed a drag event
	bool ret = false;
	static bool dragHandled = false;
	bool iDidTheHandling = false; //FIXME: this is a bit of a hack - should really split recursive/nonrecursive functionality
	if (Widget::focus && !dragHandled)
	{
		dragHandled = true;
		iDidTheHandling = true;
		Widget::focus->drag(pos - Widget::focus->position);
		ret = true;
	}
	
	//is the mouse inside the widget?
	vec2i subPos = pos - (position - getStart());
	vec2i bounds = getBounds();
	if (subPos.x >= 0 && subPos.y >= 0 && subPos.x < bounds.x && subPos.y < bounds.y)
	{
		//detect mouse entering the widget
		if (!mouseOver)
		{
			mouseOver = true;
			mouseEnter();
		}
	}
	else
	{
		//detect mouse exiting the widget
		if (mouseOver)
		{
			mouseOver = false;
			mouseLeave();
		}
	}
	
	//recurse through children
	for (WidgetList::iterator it = children.begin(); it != children.end(); ++it)
		ret = (*it)->mouseMove(pos) || ret;
	
	if (iDidTheHandling)
		dragHandled = false;
	
	return ret;
}
void Widget::capture(WidgetEvent e, WidgetCallback func)
{
	WidgetCallbackData c;
	c.w = NULL;
	c.arg = NULL;
	c.ptrv = func;
	callbacks[e] = c;
}
void Widget::capture(WidgetEvent e, WidgetCallbackArg func, void* arg)
{
	WidgetCallbackData c;
	c.w = NULL;
	c.arg = arg;
	c.ptra = func;
	callbacks[e] = c;
}
void Widget::capture(WidgetEvent e, WidgetCallbackW func)
{
	WidgetCallbackData c;
	c.w = this;
	c.arg = NULL;
	c.ptrw = func;
	callbacks[e] = c;
}
void Widget::capture(WidgetEvent e, WidgetCallbackWArg func, void* arg)
{
	WidgetCallbackData c;
	c.w = this;
	c.arg = arg;
	c.ptrwa = func;
	callbacks[e] = c;
}
void Widget::drawContent(mat44 mat)
{
	#if QUICK_WIDGET_DEBUG
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_DEPTH_TEST);
	
	vec2f a(-getStart());
	vec2f b(a + vec2f(margin.left, margin.top));
	vec2f c(b + vec2f(border.left, border.top));
	vec2f d(c + vec2f(padding.left, padding.top));
	vec2f e(d + vec2f(width, height));
	vec2f f(e + vec2f(padding.right, padding.bottom));
	vec2f g(f + vec2f(border.right, border.bottom));
	vec2f h(g + vec2f(margin.right, margin.bottom));
	
	#define BOX(ONE, TWO) mygl.begin(Immediate::TRIANGLES); mygl.vertex(ONE); mygl.vertex(TWO.x, ONE.y); mygl.vertex(TWO); mygl.vertex(ONE); mygl.vertex(TWO); mygl.vertex(ONE.x, TWO.y); mygl.end();
	
	const float alpha = 0.3;
	mygl.projection = mat * mat44::translate(0, 0, 0.1f);
	mygl.colour(1,0,0,alpha); BOX(a, h);
	mygl.colour(0,0,0,alpha); BOX(b, g);
	mygl.colour(0,0,1,alpha); BOX(c, f);
	mygl.colour(1,1,1,alpha); BOX(d, e);
	
	mygl.colour(0,1,0,alpha);
	mygl.begin(Immediate::LINE_LOOP);
	mygl.vertex(a); mygl.vertex(h.x, a.y); mygl.vertex(h); mygl.vertex(a.x, h.y);
	mygl.end();
	
	mygl.flush();
	glPopAttrib();
	#endif
}
Label::Label(const char* t) : text(Config::getString("font"), Config::get("font-size"))
{
	margin = 1;
	border = 3;
	padding = 2;

	textOffset = vec2i(0);
	center = false;
	text.colour = vec4f(0, 0.4, 0, 1.0f);
	
	text = t;
	width = max(width, text.size.x);
	height = max(height, text.size.y);
}
Label::~Label()
{
	text.release();
}
void Label::operator=(std::string str)
{
	int bw = width;
	int bh = height;
	
	text = str;
	
	width = max(width, textOffset.x + text.size.x);
	height = max(height, textOffset.y + text.size.y);
	
	//need to repack if dimensions change
	if (bw != width || bh != height)
		setDirty();
}
void Label::textf(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int bw = width;
	int bh = height;
	text.textf(fmt, ap);
	width = max(width, textOffset.x + text.size.x);
	height = max(height, textOffset.y + text.size.y);
	
	//need to repack if dimensions change
	if (bw != width || bh != height)
		setDirty();
}
void Label::drawContent(mat44 mat)
{
	Widget::drawContent(mat);
	if (center)
		mat *= mat44::translate(width / 2 - text.size.x / 2, 0, 0);
	mat *= mat44::scale(1, -1, 1); //text works with +y=up
	mat *= mat44::translate(-text.boundsMin.x + textOffset.x, -text.boundsMax.y - textOffset.y, Z + 0.1);
	text.draw(mat);
}
void Label::update()
{
	if (nineboxImage == -1)
	{
		nineboxImage = nineboxPool.create(NINEBOX_LABEL);
	}
	width = max(width, textOffset.x + text.size.x);
	height = max(height, textOffset.y + text.size.y);
	Widget::update();
}
void Label::removeBackground()
{
	if (nineboxImage >= 0)
		nineboxPool.remove(nineboxImage);
	nineboxImage = -2;
}
Slider::Slider(const char* t, float a, float b, bool decimal) : Label(t), castInt(!decimal), upper(b), lower(a)
{
	ptri = NULL;
	ptrf = NULL;
	defaults();
}
Slider::~Slider()
{
	if (sliderRail >= 0)
		nineboxPool.remove(sliderRail);
	if (sliderBar >= 0)
		nineboxPool.remove(sliderBar);
}
template<> Slider::Slider(const char* t, float a, float b, bool decimal, int* p) : Label(t), castInt(!decimal), upper(b), lower(a)
{
	ptri = p;
	ptrf = NULL;
	defaults();
}
template<> Slider::Slider(const char* t, float a, float b, bool decimal, float* p) : Label(t), castInt(!decimal), upper(b), lower(a)
{
	ptrf = p;
	ptri = NULL;
	defaults();
}
void Slider::defaults()
{
	height = 14;
	barWidth = 14;
	sliderRail = -1;
	sliderBar = -1;
	border = 3;
	padding = 4;
	margin = 1;
	maxTextWidth = 0;
	showNumber = false;
	if (ptri)
		f = (float)*ptri;
	else if (ptrf)
		f = (float)*ptrf;
	else
		f = lower;
	i = f;
}
void Slider::updateNumber()
{
	if (showNumber)
	{
		if (baseText.size() == 0 && text.text[0] != '\n')
			baseText = text.text;
		if (castInt)
			textf("%s: %i", baseText.c_str(), i);
		else
			textf("%s: %.2f", baseText.c_str(), f);
	}
}
void Slider::update()
{
	updateNumber();

	Label::update();
	maxTextWidth = mymax(maxTextWidth, text.size.x + padding.left);
	width = max(width, maxTextWidth + barWidth + 20);
	
	if (sliderRail < 0)
		sliderRail = nineboxPool.create(NINEBOX_SLIDER_SLIDE);
	if (sliderBar < 0)
		sliderBar = nineboxPool.create(NINEBOX_SLIDER_SLIDER);
}
void Slider::packed()
{
	Label::packed();
	
	if (castInt) f = i;
	
	if (sliderRail >= 0 && sliderBar >= 0)
	{
		NineBox& rail = nineboxPool[sliderRail];
		rail.border = 10;
		rail.size.x = width - maxTextWidth - rail.border.left - rail.border.right;
		rail.size.y = 2;
		rail.pos = position;
		rail.pos.x += maxTextWidth + rail.border.left;
		rail.pos.y += height/2 - rail.size.y/2;
		rail.Z = Z + 0.1f;

		NineBox& bar = nineboxPool[sliderBar];
		bar.border = 6;
		bar.size.x = barWidth - bar.border.left - bar.border.right;
		bar.size.y = padding.top + padding.bottom + height - bar.border.top - bar.border.bottom;
		bar.pos.x = position.x + maxTextWidth + bar.border.left + ((f - lower) * (rail.size.x + barWidth / 2)) / mymax(0.00001f, upper - lower);
		bar.pos.y = position.y + bar.border.top - padding.top;
		bar.Z = Z + 0.2f;
	}
	
	//vertically center text for sliders
	textOffset.y = height/2 - text.size.y/2;
}
void Slider::drag(vec2i pos)
{
	float previous = f;
	int sliderStart = maxTextWidth + barWidth / 2;
	int sliderWidth = width - maxTextWidth - barWidth;
	f = ((pos.x - sliderStart) * mymax(0.00001f, upper - lower)) / (float)sliderWidth + lower;
	f = myclamp(f, lower, upper);
	int prevI = i;
	i = round(f);
	if (castInt) f = (float)i;
	if (!castInt || prevI != i)
	{
		if (callbacks.find(SCROLL) != callbacks.end())
			callbacks[SCROLL]();
	}
	if (ptri) *ptri = i;
	if (ptrf) *ptrf = f;
	
	if (previous != f)
	{
		updateNumber();
		setDirty();
	}
}
Button::Button(const char* t) : Label(t)
{
	border = 5;
	padding = 3;
	margin = 1;
	center = true;
	subImage = imageMain;
	text.colour = vec4f(1.0);
	textClickOffset = vec2i(0);
}
void Button::mouseDown()
{
	subImage = imageClick;
	textClickOffset = vec2i(2);
	packed();
}
void Button::mouseUp()
{
	if (mouseOver) subImage = imageHover;
	else subImage = imageMain;
	textClickOffset = vec2i(0);
	packed();
}
void Button::mouseEnter()
{
	if (this == focus)
	{
		subImage = imageClick;
		textClickOffset = vec2i(2);
	}
	else subImage = imageHover;
	packed();
}
void Button::mouseLeave()
{
	if (this == focus)
	{
		subImage = imageFocus;
		textClickOffset = vec2i(0);
	}
	else subImage = imageMain;
	packed();
}
void Button::update()
{
	if (nineboxImage < 0)
	{
		nineboxImage = nineboxPool.create(NINEBOX_BUTTON);
	}
	Label::update();
}
void Button::drawContent(mat44 mat)
{
	Widget::drawContent(mat);
	//FIXME: stupid hack to add a second text offset
	vec2i tmp = textOffset;
	textOffset += textClickOffset;
	Label::drawContent(mat);
	textOffset = tmp;
}
CheckBox::CheckBox(const char* t) : Button(t)
{
	ptrb = NULL;
	iconSize = vec2i(0);
	center = false;
	b = false;
	border = 5;
	padding = 2;
	margin = 1;
	height = 12;
	
	text.colour = vec4f(1.0f);
	
	iconImage = -1;
}
CheckBox::CheckBox(const char* t, bool* p) : Button(t)
{
	ptrb = p;
	b = *ptrb;
	border = 5;
	padding = 2;
	margin = 1;
	height = 12;
	
	text.colour = vec4f(1.0f);
	
	iconImage = -1;
}
CheckBox::~CheckBox()
{
	if (iconImage >= 0)
		nineboxPool.remove(iconImage);
}
void CheckBox::updateIcon()
{
	if (iconImage >= 0)
	{
		NineBox& box = nineboxPool[iconImage];
		box.subIndex = b?1:0;
		box.border = 0;
		box.size = iconSize;
		box.pos = position + iconPos;
		box.Z = Z + 0.1f;
	}
}
void CheckBox::click()
{
	b = !b;
	if (ptrb)
		*ptrb = b;
	updateIcon();
	Button::click();
}
void CheckBox::update()
{
	if (iconImage < 0)
	{
		iconImage = nineboxPool.create(NINEBOX_CHECKBOX_CHECK);
		iconSize = nineboxPool.getSize(NINEBOX_CHECKBOX_CHECK);
	}
	
	//icons should be native-rez and centered in the left of the body + padding
	height = max(height, iconSize.y - padding.top - padding.bottom);
	
	int centerPad = padding.top + height / 2 - iconSize.y / 2;
	vec2i centerPos;
	iconPos.x = -padding.left + centerPad;
	iconPos.y = -padding.top + centerPad;
	
	textOffset.x = iconPos.x + iconSize.x + padding.left;
	
	Button::update();
}
void CheckBox::packed()
{
	updateIcon();
	Button::packed();
}
RadioButton::RadioButton(const char* t) : CheckBox(t)
{
	index = 0;
	current = -1;
}
void RadioButton::click()
{
	set();
	Widget::click();
}
void RadioButton::onAdd()
{
	assert(parent);
	bool found = false;
	int i = 0;
	for (WidgetList::const_iterator it = parent->getChildren().begin(); it != parent->getChildren().end(); ++it)
	{
		const RadioButton* other = dynamic_cast<const RadioButton*>(*it);
		if (other)
		{
			if (other == this)
				index = i; //update this radio button's index
			else if (other->b)
			{
				current = i;
				found = true; //another radio button is already checked
				b = false;
			}
			++i;
		}
	}
	if (!found)
	{
		//set(index);
		b = true;
	}
}
void RadioButton::update()
{
	if (iconImage < 0)
	{
		iconImage = nineboxPool.create(NINEBOX_RADIO_CHECK);
		iconSize = nineboxPool.getSize(NINEBOX_RADIO_CHECK);
	}
	CheckBox::update();
}
void RadioButton::set()
{
	set(index);
}
void RadioButton::set(int i)
{
	if (i < 0)
		i = index;
	
	if (i == index && b)
		return; //already set
	
	bool found = false;
	bool changed = false;
	if (parent)
	{
		for (WidgetList::const_iterator it = parent->getChildren().begin(); it != parent->getChildren().end(); ++it)
		{
			RadioButton* other = dynamic_cast<RadioButton*>(*it);
			if (other)
			{
				bool needUpdate = ((other->index == i) != other->b);
				//printf("%i == %i. %s for %i\n", other->index, other->b, needUpdate?"swap":"leave", i);
				if (needUpdate)
				{
					changed = true;
					other->b = !other->b;
					other->updateIcon();
				}
				other->current = i;
				found = found || other->b;
			}
		}
	}
	if (!found)
	{
		printf("Error: index %i not found for RadioButton. Have you added the widget yet?\n", i);
	}
	
	//let dropdown container know about the change
	if (changed)
	{
		DropDown::DropFrame* dropFrame = dynamic_cast<DropDown::DropFrame*>(parent);
		if (dropFrame)
			dropFrame->dropSelect(i);
	}
}

DropDown::DropFrame::DropFrame()
{
	margin = 0;
	padding = 0;
	border = 2;
	margin.left = 12;
}
void DropDown::DropFrame::click()
{
	//FIXME: don't want this click, only want child clicks
	owner->childClick();
}
void DropDown::DropFrame::dropSelect(int i)
{
	owner->dropSelect(i);
}
void DropDown::DropFrame::update()
{
	if (nineboxImage < 0)
	{
		nineboxImage = nineboxPool.create(NINEBOX_LABEL);
	}
	//FIXME: owner->width may be dirty - need to introduce root dependencies?
	width = max(width, owner->width);
	Widget::update();
}
DropDown::DropDown(const char* t = NULL) : Button(t)
{
	//the droplist is hidden until clicked
	center = false;
	dropListFrame.owner = this;
	dropListFrame.hide();
	dropListFrame.Z = 1.0f;
	iconImage = -1;
}
DropDown::~DropDown()
{
}
void DropDown::_add(Widget* w)
{
	dropListFrame.add(w);
	w->Z -= 0.01f;
	
	Label* l = dynamic_cast<Label*>(w);
	if (l)
		l->text.colour = vec4f(0.0f, 0.4f, 0.0f, 1.0f);
}
void DropDown::_rem(Widget* w)
{
	dropListFrame.rem(w);
}
void DropDown::update()
{
	if (iconImage < 0)
	{
		iconImage = nineboxPool.create(NINEBOX_DROPARROW);
		iconSize = nineboxPool.getSize(NINEBOX_DROPARROW) / 2;
	}
	
	height = max(height, iconSize.y - padding.top - padding.bottom);
	width = max(width, text.size.x + iconSize.x - padding.right);
	
	int centerPad = padding.top + height / 2 - iconSize.y / 2;
	vec2i centerPos;
	iconPos.x = width + padding.right - iconSize.x;
	iconPos.y = -padding.top + centerPad;

	Button::update();
	Widget::update();
}
void DropDown::updateIcon()
{
	if (iconImage >= 0)
	{
		NineBox& box = nineboxPool[iconImage];
		box.subIndex = dropListFrame.isHidden()?0:1;
		box.border = 0;
		box.size = iconSize;
		box.pos = position + iconPos;
		box.Z = Z + 0.1f;
	}
	
	if (!dropListFrame.isHidden())
	{
		vec2i p = position - getStart();
		p.y += getBounds().y;
		p.y -= margin.bottom;
		dropListFrame.setPosition(p);
	}
}
void DropDown::packed()
{
	updateIcon();
	Button::packed();
}
void DropDown::click()
{
	//hide other dropdowns if this one is being shown
	if (dropListFrame.isHidden())
	{
		for (WidgetSet::iterator it = roots->begin(); it != roots->end(); ++it)
		{
			DropDown::DropFrame* other = dynamic_cast<DropDown::DropFrame*>(*it);
			if (other)
			{
				other->hide();
				other->owner->updateIcon();
			}
		}
	}
	
	//toggle this dropdown
	dropListFrame.hide(!dropListFrame.isHidden());
	updateIcon();
	Button::click();
}
void DropDown::childClick()
{
	if (callbacks.find(CLICK) != callbacks.end())
		callbacks[CLICK]();
}
void DropDown::dropSelect(int i)
{
	if (callbacks.find(SELECT) != callbacks.end())
		callbacks[SELECT]();
}
Widget* DropDown::operator[](int i)
{
	return dropListFrame.getChildren()[i];
}
int DropDown::size()
{
	return (int)dropListFrame.getChildren().size();
}

DropSelect::DropSelect(const char* t) : DropDown(t)
{
	selected = -1;
}
DropSelect::DropSelect(const char* t, int n, ...) : DropDown(t)
{
	selected = -1;
	
	va_list args;
	va_start(args, n);
	for (int i = 0; i < n; ++i)
	{
		const char* s = va_arg(args, const char *);
		add(s);
	}
	va_end(args);
}
void DropSelect::add(const char* name)
{
	RadioButton* b = new RadioButton(name);
	b->autoDelete = true;
	add(b);
	if (selected == -1)
		set(0);
}
void DropSelect::childClick()
{
	DropDown::childClick();
	click(); //close after clicking
}
void DropSelect::dropSelect(int i)
{
	selected = i;
	DropDown::dropSelect(i);
	//printf("Selected DropSelect\n");
}
void DropSelect::set(int i)
{
	bool found = false;
	for (int c = 0; c < size(); ++c)
	{
		Widget* child = operator[](c);
		RadioButton* b = dynamic_cast<RadioButton*>(child);
		if (b)
		{
			b->set(i);
			found = true;
			break;
		}
	}
	if (!found)
	{
		printf("Error: index %i not found for DropSelect. Have you added the widget yet?\n", i);
	}
	else
	{
		selected = i;
	}
}

/*
QG::Image::Image()
{
	border = 1;
	padding = 2;
	margin = 1;
	face = -1;
	type = GL_TEXTURE_2D;
	img = 0;
	imgPtr = NULL;
}
QG::Image::Image(unsigned int& i, unsigned int t)
{
	border = 1;
	padding = 2;
	margin = 1;
	face = -1;
	type = t;
	img = 0;
	imgPtr = &i;
}
void QG::Image::set(unsigned int i)
{
	img = i;
	imgPtr = NULL;
}
void QG::Image::setType(unsigned int t)
{
	type = t;
	if (type != GL_TEXTURE_CUBE_MAP)
		face = -1;
}
void QG::Image::setFace(int i)
{
	if (type == GL_TEXTURE_CUBE_MAP)
		face = i;
}
void QG::Image::drawContent()
{
	int x = 0;
	int y = 0;
	int x2 = width;
	int y2 = height;
	
	glEnable(type);
	unsigned int target = type;
	if (imgPtr)
		glBindTexture(target, *imgPtr);
	else
		glBindTexture(target, img);

	float d = 0.9;
	mygl.colour(1,1,1,1);
	mygl.begin(Immediate::QUADS);
	switch (face)
	{
	case -1:
		mygl.texture(0, 1); mygl.vertex(x, y);
		mygl.texture(1, 1); mygl.vertex(x2, y);
		mygl.texture(1, 0); mygl.vertex(x2, y2);
		mygl.texture(0, 0); mygl.vertex(x, y2);
		break;
	case 0:
		mygl.texture(d,-1,-1); mygl.vertex(x, y2);
		mygl.texture(d,1,-1); mygl.vertex(x, y);
		mygl.texture(d,1,1); mygl.vertex(x2, y);
		mygl.texture(d,-1,1); mygl.vertex(x2, y2);
		break;
	case 1:
		mygl.texture(-d,-1,1); mygl.vertex(x, y2);
		mygl.texture(-d,1,1); mygl.vertex(x, y);
		mygl.texture(-d,1,-1); mygl.vertex(x2, y);
		mygl.texture(-d,-1,-1); mygl.vertex(x2, y2);
		break;
	case 2:
		mygl.texture(1,d,1); mygl.vertex(x, y2);
		mygl.texture(1,d,-1); mygl.vertex(x, y);
		mygl.texture(-1,d,-1); mygl.vertex(x2, y);
		mygl.texture(-1,d,1); mygl.vertex(x2, y2);
		break;
	case 3:
		mygl.texture(1,-d,1); mygl.vertex(x2, y2);
		mygl.texture(1,-d,-1); mygl.vertex(x2, y);
		mygl.texture(-1,-d,-1); mygl.vertex(x, y);
		mygl.texture(-1,-d,1); mygl.vertex(x, y2);
		break;
	case 4:
		mygl.texture(1,-1,d); mygl.vertex(x, y2);
		mygl.texture(1,1,d); mygl.vertex(x, y);
		mygl.texture(-1,1,d); mygl.vertex(x2, y);
		mygl.texture(-1,-1,d); mygl.vertex(x2, y2);
		break;
	case 5:
		mygl.texture(-1,-1,-d); mygl.vertex(x, y2);
		mygl.texture(-1,1,-d); mygl.vertex(x, y);
		mygl.texture(1,1,-d); mygl.vertex(x2, y);
		mygl.texture(1,-1,-d); mygl.vertex(x2, y2);
		break;
	}
	mygl.end();
	glDisable(type);
}
*/

}
