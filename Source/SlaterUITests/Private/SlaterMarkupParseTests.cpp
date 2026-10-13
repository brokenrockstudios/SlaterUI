// Copyright Broken Rock Studios LLC. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "SlaterMarkup.h"

namespace
{
	const TCHAR* const MkPath = TEXT("Layouts/Test.sltr");

	bool MkParse(const TCHAR* Text, FSlaterNode& Root, FSlaterError& Error, const FSlaterLimits& Limits = FSlaterLimits())
	{
		return Slater::ParseMarkup(Text, MkPath, Root, Error, Limits);
	}

	/** Empty when Text fails at exactly (Line, Column) with Fragment in the message and an empty tree; else says what happened. */
	FString MkCheckError(const TCHAR* Text, int32 Line, int32 Column, const TCHAR* Fragment, const FSlaterLimits& Limits = FSlaterLimits())
	{
		FSlaterNode Root;
		FSlaterError Error;
		if (MkParse(Text, Root, Error, Limits))
		{
			return TEXT("parsed, but an error was expected");
		}
		if (Error.Line != Line || Error.Column != Column || !Error.Message.Contains(Fragment, ESearchCase::CaseSensitive))
		{
			return FString::Printf(TEXT("got '%s', expected (%d,%d) containing '%s'"), *Error.ToString(), Line, Column, Fragment);
		}
		if (!Root.Tag.IsEmpty() || !Root.Children.IsEmpty() || !Root.Attributes.IsEmpty())
		{
			return TEXT("the failed parse left a tree behind");
		}
		return FString();
	}

	FString MkNested(int32 Depth)
	{
		FString Text;
		for (int32 I = 0; I < Depth; ++I)
		{
			Text += TEXT("<a>");
		}
		for (int32 I = 0; I < Depth; ++I)
		{
			Text += TEXT("</a>");
		}
		return Text;
	}

	/** A root with Count - 1 empty children, so Count elements in all. */
	FString MkWide(int32 Count)
	{
		FString Text = TEXT("<a>");
		for (int32 I = 1; I < Count; ++I)
		{
			Text += TEXT("<b/>");
		}
		Text += TEXT("</a>");
		return Text;
	}
}

/** Well-formed files become a tree with the right tags, attributes, text and positions. */
TEST_CLASS(SlaterParseTests, "BRS.SlaterUI.Parse")
{
	TEST_METHOD(Root_KeepsTagAttributesAndTheirOrder)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<vbox padding=\"UI.Space.M\" fill='1'/>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Tag, FString(TEXT("vbox"))));
		ASSERT_THAT(AreEqual(Root.Attributes.Num(), 2));
		ASSERT_THAT(AreEqual(Root.Attributes[0].Name, FString(TEXT("padding"))));
		ASSERT_THAT(AreEqual(Root.Attributes[0].Value, FString(TEXT("UI.Space.M"))));
		ASSERT_THAT(AreEqual(Root.Attributes[1].Name, FString(TEXT("fill"))));
		ASSERT_THAT(AreEqual(Root.Attributes[1].Value, FString(TEXT("1"))));
		ASSERT_THAT(IsTrue(Root.Children.IsEmpty()));
	}

	TEST_METHOD(Positions_AreOneBasedForElementsAndAttributes)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<vbox padding=\"UI.Space.M\" fill='1'/>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Pos.Line, 1));
		ASSERT_THAT(AreEqual(Root.Pos.Column, 1));
		ASSERT_THAT(AreEqual(Root.Attributes[0].Pos.Column, 7));
		ASSERT_THAT(AreEqual(Root.Attributes[1].Pos.Column, 28));
	}

	TEST_METHOD(Nesting_BuildsTheTreeWithPositions)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<vbox>\n  <hbox>\n    <text value=\"a\"/>\n    <text value=\"b\"/>\n  </hbox>\n  <spacer/>\n</vbox>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Children.Num(), 2));

		const FSlaterNode& Hbox = Root.Children[0];
		ASSERT_THAT(AreEqual(Hbox.Tag, FString(TEXT("hbox"))));
		ASSERT_THAT(AreEqual(Hbox.Pos.Line, 2));
		ASSERT_THAT(AreEqual(Hbox.Pos.Column, 3));
		ASSERT_THAT(AreEqual(Hbox.Children.Num(), 2));
		ASSERT_THAT(AreEqual(Hbox.Children[1].Pos.Line, 4));
		ASSERT_THAT(AreEqual(Hbox.Children[1].Pos.Column, 5));
		ASSERT_THAT(AreEqual(Hbox.Children[1].Attributes[0].Value, FString(TEXT("b"))));

		ASSERT_THAT(AreEqual(Root.Children[1].Tag, FString(TEXT("spacer"))));
		ASSERT_THAT(AreEqual(Root.Children[1].Pos.Line, 6));
	}

	TEST_METHOD(EmptyElementPair_HasNoChildrenAndNoText)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a></a >"), Root, Error)));
		ASSERT_THAT(IsTrue(Root.Children.IsEmpty()));
		ASSERT_THAT(IsTrue(Root.Text.IsEmpty()));
	}

	TEST_METHOD(Text_IsTrimmedAndKnowsWhereItStarts)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<text>  Hello  </text>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Text, FString(TEXT("Hello"))));
		ASSERT_THAT(AreEqual(Root.TextPos.Line, 1));
		ASSERT_THAT(AreEqual(Root.TextPos.Column, 9));
	}

	TEST_METHOD(Text_AroundChildren_IsJoinedWithOneSpace)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<p>one<b/>two</p>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Text, FString(TEXT("one two"))));
		ASSERT_THAT(AreEqual(Root.Children.Num(), 1));
	}

	TEST_METHOD(Text_OfOnlyWhitespace_IsDropped)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a>\n  <b/>\n</a>"), Root, Error)));
		ASSERT_THAT(IsTrue(Root.Text.IsEmpty()));
		ASSERT_THAT(AreEqual(Root.TextPos.Line, 0));
	}

	TEST_METHOD(Comments_AreSkippedAndKeepLineNumbersRight)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<!-- header\n  two lines -->\n<a>\n  <!-- <b/> -->\n  <c/>\n</a>\n<!-- trailer -->"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Tag, FString(TEXT("a"))));
		ASSERT_THAT(AreEqual(Root.Pos.Line, 3));
		ASSERT_THAT(AreEqual(Root.Children.Num(), 1));
		ASSERT_THAT(AreEqual(Root.Children[0].Tag, FString(TEXT("c"))));
		ASSERT_THAT(AreEqual(Root.Children[0].Pos.Line, 5));
		ASSERT_THAT(AreEqual(Root.Children[0].Pos.Column, 3));
	}

	TEST_METHOD(XmlDeclaration_AtTheStart_IsAccepted)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<a/>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Pos.Line, 2));
		ASSERT_THAT(AreEqual(Root.Pos.Column, 1));
	}

	TEST_METHOD(ByteOrderMark_IsSkippedAndDoesNotShiftColumns)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("\xFEFF<a/>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Pos.Line, 1));
		ASSERT_THAT(AreEqual(Root.Pos.Column, 1));
		ASSERT_THAT(IsTrue(MkParse(TEXT("\xFEFF<?xml version=\"1.0\"?><a/>"), Root, Error)));
	}

	TEST_METHOD(LineBreaks_CrLfAndLoneCr_CountOnce)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a>\r\n  <b/>\r\n</a>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Children[0].Pos.Line, 2));
		ASSERT_THAT(AreEqual(Root.Children[0].Pos.Column, 3));
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a>\r  <b/>\r</a>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Children[0].Pos.Line, 2));
		ASSERT_THAT(AreEqual(Root.Children[0].Pos.Column, 3));
	}

	TEST_METHOD(AttributeValues_AcceptEitherQuoteAndTurnWhitespaceIntoSpaces)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a x=\"it's\" y='say \"hi\"' z=\"a\tb\nc\" w=\"a&#10;b\"/>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.FindAttribute(TEXT("x"))->Value, FString(TEXT("it's"))));
		ASSERT_THAT(AreEqual(Root.FindAttribute(TEXT("y"))->Value, FString(TEXT("say \"hi\""))));
		ASSERT_THAT(AreEqual(Root.FindAttribute(TEXT("z"))->Value, FString(TEXT("a b c"))));
		ASSERT_THAT(AreEqual(Root.FindAttribute(TEXT("w"))->Value, FString(TEXT("a\nb"))));
	}

	TEST_METHOD(Names_AllowDashDotAndColon_AndLookupIsCaseSensitive)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<uniform-grid on-click=\"Close\" param:Progress=\"0.5\"/>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Tag, FString(TEXT("uniform-grid"))));
		ASSERT_THAT(IsNotNull(Root.FindAttribute(TEXT("on-click"))));
		ASSERT_THAT(IsNotNull(Root.FindAttribute(TEXT("param:Progress"))));
		ASSERT_THAT(IsNull(Root.FindAttribute(TEXT("param:progress"))));
		ASSERT_THAT(IsNull(Root.FindAttribute(TEXT("missing"))));
	}

	TEST_METHOD(SameAttributeNameOnDifferentElements_IsFine_AndCaseDistinguishesAttributes)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a x=\"1\" X=\"2\"><b x=\"3\"/></a>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Attributes.Num(), 2));
		ASSERT_THAT(AreEqual(Root.Children[0].Attributes[0].Value, FString(TEXT("3"))));
	}
};

/** Entities, in attribute values and in text. */
TEST_CLASS(SlaterEntityTests, "BRS.SlaterUI.Parse.Entity")
{
	TEST_METHOD(Named_AreDecodedInValuesAndText)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a v=\"&amp;&lt;&gt;&quot;&apos;\">&amp;&lt;&gt;&quot;&apos;</a>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Attributes[0].Value, FString(TEXT("&<>\"'"))));
		ASSERT_THAT(AreEqual(Root.Text, FString(TEXT("&<>\"'"))));
	}

	TEST_METHOD(AmpersandText_IsNotDecodedTwice)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a>&amp;lt;</a>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Text, FString(TEXT("&lt;"))));
	}

	TEST_METHOD(Numeric_DecimalAndHex)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a v=\"&#65;&#x42;&#x63;&#x4a;&#x4A;\"/>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Attributes[0].Value, FString(TEXT("ABcJJ"))));
	}

	TEST_METHOD(Numeric_BeyondTheBasicPlane_BecomesASurrogatePair)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a>&#x1F600;</a>"), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Text.Len(), 2));
		ASSERT_THAT(IsTrue(Root.Text[0] == 0xD83D));
		ASSERT_THAT(IsTrue(Root.Text[1] == 0xDE00));
	}

	TEST_METHOD(Unknown_NamesTheEntity_AtTheAmpersand)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>x &nbsp; y</a>"), 1, 6, TEXT("unknown entity '&nbsp;'")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a v=\"&AMP;\"/>"), 1, 7, TEXT("unknown entity '&AMP;'")), FString()));
	}

	TEST_METHOD(BareAmpersand_IsRefused_WithTheFix)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>a & b</a>"), 1, 6, TEXT("write &amp;")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>Tom &amp Jerry</a>"), 1, 8, TEXT("write &amp;")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a v=\"a & b\"/>"), 1, 9, TEXT("write &amp;")), FString()));
	}

	TEST_METHOD(BadNumbers_AreRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>&#;</a>"), 1, 4, TEXT("has no digits")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>&#x;</a>"), 1, 4, TEXT("has no digits")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>&#xZZ;</a>"), 1, 4, TEXT("is not a number")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>&#0;</a>"), 1, 4, TEXT("not a character XML allows")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>&#xD800;</a>"), 1, 4, TEXT("not a character XML allows")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>&#1114112;</a>"), 1, 4, TEXT("not a character XML allows")), FString()));
	}
};

/** Every way a file can be malformed fails with the line and column of the mistake. */
TEST_CLASS(SlaterErrorTests, "BRS.SlaterUI.Parse.Error")
{
	TEST_METHOD(DuplicateAttribute_PointsAtTheSecond_AndNamesTheFirst)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>\n  <b x=\"1\" y=\"2\" x=\"3\"/>\n</a>"), 2, 18, TEXT("duplicate attribute 'x' on <b> (first written at line 2, column 6)")), FString()));
	}

	TEST_METHOD(MismatchedClosingTag_NamesTheOpenOne)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>\n  <b>\n</a>"), 3, 1, TEXT("closing tag </a> does not match <b> opened at line 2, column 3")), FString()));
	}

	TEST_METHOD(ClosingTag_IsCaseSensitive)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<A></a>"), 1, 4, TEXT("does not match <A>")), FString()));
	}

	TEST_METHOD(ClosingTagWithNothingOpen_IsRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("</a>"), 1, 1, TEXT("has no opening tag")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a/></a>"), 1, 5, TEXT("has no opening tag")), FString()));
	}

	TEST_METHOD(UnclosedElement_PointsAtItsOpeningTag)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>\n  <b>\n    <c/>\n"), 2, 3, TEXT("<b> is never closed")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>"), 1, 1, TEXT("<a> is never closed")), FString()));
	}

	TEST_METHOD(TagCutOffByTheEndOfTheFile_PointsAtItsStart)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>\n<b x=\"1\" "), 2, 1, TEXT("<b> is never finished")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a"), 1, 1, TEXT("<a> is never finished")), FString()));
	}

	TEST_METHOD(SecondRoot_IsRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a/>\n<b/>"), 2, 1, TEXT("<b> is a second one")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a></a><b/>"), 1, 8, TEXT("<b> is a second one")), FString()));
	}

	TEST_METHOD(TextOutsideTheRoot_IsRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("hello <a/>"), 1, 1, TEXT("text before the root element")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a/> tail"), 1, 6, TEXT("text after the root element")), FString()));
	}

	TEST_METHOD(NoRoot_IsRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT(""), 1, 1, TEXT("no root element")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("   \n  "), 2, 3, TEXT("no root element")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<!-- only a comment -->"), 1, 24, TEXT("no root element")), FString()));
	}

	TEST_METHOD(UnquotedAttributeValue_IsRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a x=1/>"), 1, 6, TEXT("must be in quotes")), FString()));
	}

	TEST_METHOD(AttributeWithoutValue_IsRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a enabled/>"), 1, 4, TEXT("attribute 'enabled' has no value")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a enabled>"), 1, 4, TEXT("attribute 'enabled' has no value")), FString()));
	}

	TEST_METHOD(AttributesWithoutASpaceBetween_AreRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a x=\"1\"y=\"2\"/>"), 1, 9, TEXT("expected a space")), FString()));
	}

	TEST_METHOD(UnterminatedAttributeValue_PointsAtTheOpeningQuote)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a x=\"1"), 1, 6, TEXT("value of 'x' is never closed")), FString()));
	}

	TEST_METHOD(LessThanInAValue_SuggestsTheEntityAndTheMissingQuote)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a x=\"<b/>\"/>"), 1, 7, TEXT("write &lt;, or a closing \" is missing")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a x=\"1>\n<b/>"), 2, 1, TEXT("'<' cannot appear in the value of 'x' (starts at line 1, column 6)")), FString()));
	}

	TEST_METHOD(MalformedTags_AreRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("< a/>"), 1, 2, TEXT("expected a tag name after '<'")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<1a/>"), 1, 2, TEXT("expected a tag name after '<'")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a/ >"), 1, 3, TEXT("expected '>' right after '/'")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a!/>"), 1, 3, TEXT("expected a space, '>' or '/>'")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a></ a>"), 1, 6, TEXT("expected a tag name after '</'")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a></a x=\"1\">"), 1, 8, TEXT("expected '>' to end </a>")), FString()));
	}

	TEST_METHOD(UnsupportedConstructs_AreRefusedByName)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<!DOCTYPE a>\n<a/>"), 1, 1, TEXT("DOCTYPE")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a><![CDATA[x]]></a>"), 1, 4, TEXT("CDATA")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a><?php ?></a>"), 1, 4, TEXT("processing instructions are not supported")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT(" <?xml version=\"1.0\"?><a/>"), 1, 2, TEXT("only allowed at the very start")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<?xml version=\"1.0\"<a/>"), 1, 1, TEXT("never closed")), FString()));
	}

	TEST_METHOD(UnterminatedComment_PointsAtItsStart)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>\n  <!-- x </a>"), 2, 3, TEXT("comment is never closed")), FString()));
	}

	TEST_METHOD(ControlCharacters_AreRefused)
	{
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>\x01</a>"), 1, 4, TEXT("control character U+0001")), FString()));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a x=\"\x02\"/>"), 1, 7, TEXT("control character U+0002")), FString()));
	}

	TEST_METHOD(ErrorText_IsPathLineColumnMessage)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsFalse(MkParse(TEXT("<a>\n  <b>\n</a>"), Root, Error)));
		ASSERT_THAT(IsTrue(Error.ToString().StartsWith(TEXT("Layouts/Test.sltr(3,1): closing tag </a>"))));

		FSlaterError Unnamed;
		Unnamed.Line = 4;
		Unnamed.Column = 2;
		Unnamed.Message = TEXT("boom");
		ASSERT_THAT(AreEqual(Unnamed.ToString(), FString(TEXT("<markup>(4,2): boom"))));
	}

	TEST_METHOD(AFailedParse_ClearsWhatAnEarlierParseLeft)
	{
		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a><b/></a>"), Root, Error)));
		ASSERT_THAT(IsFalse(MkParse(TEXT("<a><b></a>"), Root, Error)));
		ASSERT_THAT(IsTrue(Root.Tag.IsEmpty()));
		ASSERT_THAT(IsTrue(Root.Children.IsEmpty()));
		ASSERT_THAT(IsTrue(MkParse(TEXT("<c/>"), Root, Error)));
		ASSERT_THAT(IsTrue(Error.Message.IsEmpty()));
	}
};

/** Size, depth and node-count caps fail like syntax errors, and exactly at the cap still parses. */
TEST_CLASS(SlaterLimitTests, "BRS.SlaterUI.Parse.Limits")
{
	TEST_METHOD(FileSize_AtTheCapParses_OneOverFails)
	{
		FSlaterNode Root;
		FSlaterError Error;
		FSlaterLimits Limits;
		Limits.MaxChars = 8;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a>1</a>"), Root, Error, Limits)));
		Limits.MaxChars = 7;
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a>1</a>"), 1, 1, TEXT("8 characters long; the limit is 7"), Limits), FString()));
	}

	TEST_METHOD(Depth_AtTheCapParses_OneOverFailsAtTheDeepestTag)
	{
		FSlaterNode Root;
		FSlaterError Error;
		FSlaterLimits Limits;
		Limits.MaxDepth = 3;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a><b><c/></b></a>"), Root, Error, Limits)));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a><b><c><d/></c></b></a>"), 1, 10, TEXT("nested deeper than 3 levels"), Limits), FString()));
	}

	TEST_METHOD(Nodes_AtTheCapParse_OneOverFailsAtTheExtraElement)
	{
		FSlaterNode Root;
		FSlaterError Error;
		FSlaterLimits Limits;
		Limits.MaxNodes = 3;
		ASSERT_THAT(IsTrue(MkParse(TEXT("<a><b/><c/></a>"), Root, Error, Limits)));
		ASSERT_THAT(AreEqual(MkCheckError(TEXT("<a><b/><c/><d/></a>"), 1, 12, TEXT("more than 3 elements"), Limits), FString()));
	}

	TEST_METHOD(DefaultCaps_AreThirtyTwoLevelsAnd4096Elements)
	{
		const FSlaterLimits Defaults = FSlaterLimits();
		ASSERT_THAT(AreEqual(Defaults.MaxDepth, 32));
		ASSERT_THAT(AreEqual(Defaults.MaxNodes, 4096));

		FSlaterNode Root;
		FSlaterError Error;
		ASSERT_THAT(IsTrue(MkParse(*MkNested(32), Root, Error)));
		ASSERT_THAT(IsFalse(MkParse(*MkNested(33), Root, Error)));
		ASSERT_THAT(IsTrue(Error.Message.Contains(TEXT("nested deeper than 32 levels"))));

		ASSERT_THAT(IsTrue(MkParse(*MkWide(4096), Root, Error)));
		ASSERT_THAT(AreEqual(Root.Children.Num(), 4095));
		ASSERT_THAT(IsFalse(MkParse(*MkWide(4097), Root, Error)));
		ASSERT_THAT(IsTrue(Error.Message.Contains(TEXT("more than 4096 elements"))));
	}

	TEST_METHOD(Depth_IsOnlyAPolicy_AThousandLevelsDoNotNeedAThousandStackFrames)
	{
		FSlaterNode Root;
		FSlaterError Error;
		FSlaterLimits Limits;
		Limits.MaxDepth = 5000;
		ASSERT_THAT(IsTrue(MkParse(*MkNested(1000), Root, Error, Limits)));

		int32 Depth = 0;
		for (const FSlaterNode* Node = &Root; Node; Node = Node->Children.IsEmpty() ? nullptr : &Node->Children[0])
		{
			++Depth;
		}
		ASSERT_THAT(AreEqual(Depth, 1000));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
