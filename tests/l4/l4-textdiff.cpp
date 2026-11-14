/* -----------------------------------------------------------------------------
 * CyberiadaPro, the Cyberiada State Machine Editor
 * -----------------------------------------------------------------------------
 *
 * The line diff test of the text diff tab (see docs/TESTING.md, L4)
 *
 * Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see https://www.gnu.org/licenses/
 *
 * ----------------------------------------------------------------------------- */

#include <QtTest>
#include <random>
#include <vector>
#include "text_diff.h"

using namespace TextDiff;

// the random texts of the property test
static const int RANDOM_CASES = 300;
static const int RANDOM_MAX_LINES = 40;
static const int RANDOM_ALPHABET = 6;
static const unsigned RANDOM_SEED = 1044;
// the large texts of the budget test
static const int LARGE_LINES = 3000;
static const qint64 LARGE_TIME_MS = 10000;

// EDIT-MODE-20..23: the rows, the pairs, the changed characters and the hunks
class TestTextDiff: public QObject {
	Q_OBJECT

private slots:
	void test_split_lines();
	void test_equal();
	void test_added_removed();
	void test_changed_pair();
	void test_words();
	void test_not_similar();
	void test_pair_ahead();
	void test_ignore_whitespace();
	void test_empty_sides();
	void test_minimal();
	void test_large();

private:
	// the rows as "<kind><base>,<target>" from 1, a filler as "."
	static QStringList rows(const Result& result);
	static QString spanText(const QString& line, const Spans& spans);
	// both texts given back by the rows, in order
	static bool consistent(const QStringList& base, const QStringList& target, const Result& result);
	static int lcs(const QStringList& a, const QStringList& b);
};

QStringList TestTextDiff::rows(const Result& result)
{
	static const char kinds[] = { '=', '-', '+', '~' };
	QStringList list;
	for (const Row& row : result.rows) {
		list << QString("%1%2,%3").arg(kinds[row.kind])
			.arg(row.base < 0 ? QString(".") : QString::number(row.base + 1))
			.arg(row.target < 0 ? QString(".") : QString::number(row.target + 1));
	}
	return list;
}

QString TestTextDiff::spanText(const QString& line, const Spans& spans)
{
	QStringList parts;
	for (const Span& span : spans) parts << line.mid(span.from, span.to - span.from);
	return parts.join("|");
}

bool TestTextDiff::consistent(const QStringList& base, const QStringList& target, const Result& result)
{
	int i = 0, j = 0;
	for (const Row& row : result.rows) {
		if (row.base >= 0 && row.base != i++) return false;
		if (row.target >= 0 && row.target != j++) return false;
		if (row.kind == rowEqual && base[row.base] != target[row.target]) return false;
		if ((row.kind == rowRemoved) != (row.target < 0 && row.base >= 0)) return false;
		if ((row.kind == rowAdded) != (row.base < 0 && row.target >= 0)) return false;
	}
	return i == base.size() && j == target.size();
}

int TestTextDiff::lcs(const QStringList& a, const QStringList& b)
{
	std::vector<std::vector<int>> t(size_t(a.size() + 1), std::vector<int>(size_t(b.size() + 1), 0));
	for (int i = a.size() - 1; i >= 0; i--) {
		for (int j = b.size() - 1; j >= 0; j--) {
			t[size_t(i)][size_t(j)] = a[i] == b[j] ? t[size_t(i + 1)][size_t(j + 1)] + 1
			                                       : qMax(t[size_t(i + 1)][size_t(j)], t[size_t(i)][size_t(j + 1)]);
		}
	}
	return t[0][0];
}

void TestTextDiff::test_split_lines()
{
	QCOMPARE(splitLines(""), QStringList());
	QCOMPARE(splitLines("\n"), QStringList() << "");
	QCOMPARE(splitLines("a\nb"), QStringList() << "a" << "b");
	QCOMPARE(splitLines("a\r\nb\rc\n"), QStringList() << "a" << "b" << "c");
	QCOMPARE(splitLines("a\n\nb\n"), QStringList() << "a" << "" << "b");
}

void TestTextDiff::test_equal()
{
	QStringList text = QStringList() << "one" << "two" << "three";
	Result r = diffLines(text, text);
	QCOMPARE(rows(r), QStringList() << "=1,1" << "=2,2" << "=3,3");
	QVERIFY(r.hunks.isEmpty());
	QCOMPARE(r.added + r.removed + r.changed, 0);
}

void TestTextDiff::test_added_removed()
{
	QStringList base = QStringList() << "a" << "b" << "c";
	QStringList target = QStringList() << "a" << "c" << "d";
	Result r = diffLines(base, target);
	QCOMPARE(rows(r), QStringList() << "=1,1" << "-2,." << "=3,2" << "+.,3");
	QCOMPARE(r.hunks.size(), 2);
	QCOMPARE(r.hunks[0].firstRow, 1);
	QCOMPARE(r.hunks[0].lastRow, 2);
	QCOMPARE(r.hunks[0].baseLine, 1);
	QCOMPARE(r.hunks[0].baseCount, 1);
	QCOMPARE(r.hunks[0].targetLine, 1);
	QCOMPARE(r.hunks[0].targetCount, 0);
	QCOMPARE(r.hunks[1].baseLine, 3);
	QCOMPARE(r.hunks[1].baseCount, 0);
	QCOMPARE(r.hunks[1].targetLine, 2);
	QCOMPARE(r.hunks[1].targetCount, 1);
	QCOMPARE(r.removed, 1);
	QCOMPARE(r.added, 1);
}

void TestTextDiff::test_changed_pair()
{
	QStringList base = QStringList() << "void step() {" << "    n += 1;" << "}";
	QStringList target = QStringList() << "void step() {" << "    n += 2;" << "    log();" << "}";
	Result r = diffLines(base, target);
	QCOMPARE(rows(r), QStringList() << "=1,1" << "~2,2" << "+.,3" << "=3,4");
	QCOMPARE(spanText(base[1], r.rows[1].baseSpans), QString("1"));
	QCOMPARE(spanText(target[1], r.rows[1].targetSpans), QString("2"));
	QCOMPARE(r.hunks.size(), 1);
	QCOMPARE(r.hunks[0].baseCount, 1);
	QCOMPARE(r.hunks[0].targetCount, 2);
	QCOMPARE(r.changed, 1);
	QCOMPARE(r.added, 1);
}

// the changed characters are extended to the words they touch
void TestTextDiff::test_words()
{
	Result r = diffLines(QStringList() << "value = count + 1", QStringList() << "value = counter + 1");
	QCOMPARE(rows(r), QStringList() << "~1,1");
	QCOMPARE(spanText("value = count + 1", r.rows[0].baseSpans), QString(""));
	QCOMPARE(spanText("value = counter + 1", r.rows[0].targetSpans), QString("counter"));
	r = diffLines(QStringList() << "get_light() < DAY", QStringList() << "sensor.get() < DAY");
	QCOMPARE(rows(r), QStringList() << "~1,1");
	QCOMPARE(spanText("get_light() < DAY", r.rows[0].baseSpans), QString("get_light"));
	QCOMPARE(spanText("sensor.get() < DAY", r.rows[0].targetSpans), QString("sensor."));
}

void TestTextDiff::test_not_similar()
{
	Result r = diffLines(QStringList() << "alpha", QStringList() << "zzzz");
	QCOMPARE(rows(r), QStringList() << "-1,." << "+.,1");
	QCOMPARE(r.hunks.size(), 1);
	QCOMPARE(r.changed, 0);
}

// a base line pairs with a similar target line after inserted ones
void TestTextDiff::test_pair_ahead()
{
	QStringList base = QStringList() << "begin" << "total = sum(a)" << "end";
	QStringList target = QStringList() << "begin" << "check(a)" << "total = sum(b)" << "end";
	Result r = diffLines(base, target);
	QCOMPARE(rows(r), QStringList() << "=1,1" << "+.,2" << "~2,3" << "=3,4");
}

void TestTextDiff::test_ignore_whitespace()
{
	QCOMPARE(normalise("  a \t b  "), QString("a b"));
	QStringList base = QStringList() << "if (x)  {" << "\treturn 1;";
	QStringList target = QStringList() << "if (x) {" << "    return  2;  ";
	Options options;
	QCOMPARE(rows(diffLines(base, target)), QStringList() << "~1,1" << "~2,2");
	options.ignoreWhitespace = true;
	Result r = diffLines(base, target, options);
	QCOMPARE(rows(r), QStringList() << "=1,1" << "~2,2");
	QCOMPARE(spanText(base[1], r.rows[1].baseSpans), QString("1"));
	QCOMPARE(spanText(target[1], r.rows[1].targetSpans), QString("2"));
}

void TestTextDiff::test_empty_sides()
{
	QStringList text = QStringList() << "a" << "b";
	Result r = diffLines(QStringList(), text);
	QCOMPARE(rows(r), QStringList() << "+.,1" << "+.,2");
	QCOMPARE(r.hunks.size(), 1);
	QCOMPARE(r.hunks[0].baseCount, 0);
	r = diffLines(text, QStringList());
	QCOMPARE(rows(r), QStringList() << "-1,." << "-2,.");
	QVERIFY(diffLines(QStringList(), QStringList()).rows.isEmpty());
}

// the equal rows are a longest common subsequence: the diff is minimal
void TestTextDiff::test_minimal()
{
	std::mt19937 random(RANDOM_SEED);
	std::uniform_int_distribution<int> length(0, RANDOM_MAX_LINES);
	std::uniform_int_distribution<int> letter(0, RANDOM_ALPHABET - 1);
	for (int c = 0; c < RANDOM_CASES; c++) {
		QStringList a, b;
		for (int n = length(random); n > 0; n--) a << QString(QChar('a' + letter(random)));
		for (int n = length(random); n > 0; n--) b << QString(QChar('a' + letter(random)));
		Result r = diffLines(a, b);
		QVERIFY2(consistent(a, b, r), qPrintable(a.join("") + " / " + b.join("")));
		int equal = 0;
		for (const Row& row : r.rows) {
			if (row.kind == rowEqual) equal++;
		}
		QVERIFY2(equal == lcs(a, b), qPrintable(a.join("") + " / " + b.join("")));
	}
}

// texts of no common line end in time, every line kept
void TestTextDiff::test_large()
{
	QStringList a, b;
	for (int i = 0; i < LARGE_LINES; i++) {
		a << QString("first %1").arg(i);
		b << QString("second line %1 of another text").arg(i * 7);
	}
	QElapsedTimer timer;
	timer.start();
	Result r = diffLines(a, b);
	QVERIFY(consistent(a, b, r));
	QCOMPARE(r.removed + r.changed, LARGE_LINES);
	QVERIFY(timer.elapsed() < LARGE_TIME_MS);
	// an edit in the middle of a long text is one hunk
	QStringList edited = a;
	edited[LARGE_LINES / 2] = "first changed";
	r = diffLines(a, edited);
	QCOMPARE(r.hunks.size(), 1);
	QCOMPARE(r.changed, 1);
}

QTEST_MAIN(TestTextDiff)
#include "l4-textdiff.moc"
