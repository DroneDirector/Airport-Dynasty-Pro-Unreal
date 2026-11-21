// Words to the voice model's sounds in Unreal (G2P.h). British only (the game's voices are British; misaki's gb lexicon).
#include "G2P.h"
#include "ADPLog.h"
#include "Internationalization/Regex.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "G2PTables.inl"

namespace ADPG2P
{
	static FCriticalSection Lock;
	static TMap<FString, FString> Lex;     // misaki's British dictionary: word -> phonemes
	static TMap<FString, FString> Names;   // names: how they are said, in plain spelling
	static TMap<TCHAR, int64> IdOf;
	static int32 State = 0;                // 0 not tried, 1 ready, -1 not there

	static const TCHAR* Ones[] = {TEXT("zero"), TEXT("one"), TEXT("two"), TEXT("three"), TEXT("four"), TEXT("five"), TEXT("six"), TEXT("seven"), TEXT("eight"), TEXT("nine"), TEXT("ten"), TEXT("eleven"), TEXT("twelve"), TEXT("thirteen"), TEXT("fourteen"), TEXT("fifteen"), TEXT("sixteen"), TEXT("seventeen"), TEXT("eighteen"), TEXT("nineteen")};
	static const TCHAR* Tens[] = {TEXT(""), TEXT(""), TEXT("twenty"), TEXT("thirty"), TEXT("forty"), TEXT("fifty"), TEXT("sixty"), TEXT("seventy"), TEXT("eighty"), TEXT("ninety")};

	bool Ready()
	{
		FScopeLock G(&Lock);
		if (State) return State > 0;
		State = -1;
		TArray<uint8> Gz;
		if (!FFileHelper::LoadFileToArray(Gz, *(FPaths::ProjectContentDir() / TEXT("Voice/lexicon-gb.json.gz"))) || Gz.Num() < 8) { ADP_WARN(TEXT("G2P: no lexicon-gb.json.gz")); return false; }
		const uint32 Size = uint32(Gz[Gz.Num() - 4]) | (uint32(Gz[Gz.Num() - 3]) << 8) | (uint32(Gz[Gz.Num() - 2]) << 16) | (uint32(Gz[Gz.Num() - 1]) << 24);
		TArray<uint8> Raw; Raw.SetNumUninitialized(Size + 1);
		if (!FCompression::UncompressMemory(NAME_Gzip, Raw.GetData(), int64(Size), Gz.GetData(), Gz.Num())) { ADP_WARN(TEXT("G2P: the lexicon didn't unpack")); return false; }
		Raw[Size] = 0;
		const FString Json = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Raw.GetData())));
		TSharedPtr<FJsonObject> O;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O) { ADP_WARN(TEXT("G2P: the lexicon didn't read")); return false; }
		Lex.Reserve(O->Values.Num());
		for (const TPair<FString, TSharedPtr<FJsonValue>>& P : O->Values)
		{
			FString V;
			if (P.Value->TryGetString(V)) Lex.Add(P.Key, V);
			else if (const TSharedPtr<FJsonObject>* Ob; P.Value->TryGetObject(Ob) && (*Ob)->TryGetStringField(TEXT("DEFAULT"), V)) Lex.Add(P.Key, V);
		}
		for (const ADPG2PTables::FPair& N : ADPG2PTables::Names) Names.Add(N.A, N.B);
		for (const ADPG2PTables::FId& I : ADPG2PTables::KokoroIds) IdOf.Add(TCHAR(I.C), I.Id);
		ADP_LOG(TEXT("G2P: %d words in the British dictionary"), Lex.Num());
		State = 1;
		return true;
	}

	// ---------------------------------------------------------------- numbers
	static FString Num(int64 N)
	{
		N = FMath::Abs(N);
		if (N < 20) return Ones[N];
		if (N < 100) return FString(Tens[N / 10]) + (N % 10 ? FString(TEXT(" ")) + Ones[N % 10] : FString());
		if (N < 1000) return FString(Ones[N / 100]) + TEXT(" hundred") + (N % 100 ? TEXT(" and ") + Num(N % 100) : FString());
		const int64 V[] = {1000000000, 1000000, 1000}; const TCHAR* W[] = {TEXT("billion"), TEXT("million"), TEXT("thousand")};
		for (int32 k = 0; k < 3; ++k) if (N >= V[k]) { const int64 R = N % V[k]; return Num(N / V[k]) + TEXT(" ") + W[k] + (R ? (R < 100 ? TEXT(" and ") : TEXT(" ")) + Num(R) : FString()); }
		return FString::Printf(TEXT("%lld"), N);
	}
	static FString Digits(const FString& S) { FString O; for (const TCHAR C : S) if (FChar::IsDigit(C)) { if (!O.IsEmpty()) O += TEXT(" "); O += Ones[C - '0']; } return O; }
	static FString Model(const FString& S)
	{
		if (S.Len() == 3 && S[0] == '7' && S[2] == '7') return S == TEXT("777") ? TEXT("triple seven") : Digits(S);
		if (S.Len() == 3 && S[1] == '0' && S[2] == '0') return FString(Ones[S[0] - '0']) + TEXT(" hundred");
		if (S.Len() == 3) return FString(Ones[S[0] - '0']) + TEXT(" ") + (S[1] == '0' ? FString(TEXT("oh ")) + Ones[S[2] - '0'] : Num(FCString::Atoi(*S.Mid(1))));
		return Digits(S);
	}
	static FString Spaced(const FString& S) { FString O; for (const TCHAR C : S) { if (!O.IsEmpty()) O += TEXT(" "); O.AppendChar(C); } return O; }

	// a regular expression's every match replaced by what F makes of it
	static FString Rx(const FString& In, const TCHAR* Pat, TFunctionRef<FString(FRegexMatcher&)> F)
	{
		const FRegexPattern P(Pat); FRegexMatcher M(P, In); FString Out; int32 Last = 0;
		while (M.FindNext()) { Out += In.Mid(Last, M.GetMatchBeginning() - Last); Out += F(M); Last = M.GetMatchEnding(); }
		Out += In.Mid(Last);
		return Out;
	}
	static FString Grp(FRegexMatcher& M, int32 I) { return M.GetCaptureGroupBeginning(I) == INDEX_NONE ? FString() : M.GetCaptureGroup(I); }

	/** Writes out what the dictionary can't read: money, numbers, codes, units (the game's normalise, British). */
	static FString Normalise(const FString& Text)
	{
		FString T = TEXT(" ") + Text.Replace(TEXT("‘"), TEXT("'")).Replace(TEXT("’"), TEXT("'")).Replace(TEXT("“"), TEXT("\"")).Replace(TEXT("”"), TEXT("\"")) + TEXT(" ");
		T = Rx(T, TEXT("\\s+"), [](FRegexMatcher&) { return FString(TEXT(" ")); });
		// money
		T = Rx(T, TEXT("([-+−]?)\\$\\s?(\\d[\\d,]*)(?:\\.(\\d+))?\\s?([kKMB])?\\b"), [](FRegexMatcher& M)
		{
			const int64 N = FCString::Atoi64(*Grp(M, 2).Replace(TEXT(","), TEXT(""))); const FString Sg = Grp(M, 1), Dec = Grp(M, 3), Mu = Grp(M, 4);
			const FString Head = (Sg == TEXT("-") || Sg == TEXT("−")) ? TEXT("minus ") : TEXT("");
			if (!Mu.IsEmpty()) { const TCHAR* W = Mu.ToLower() == TEXT("k") ? TEXT("thousand") : Mu == TEXT("M") ? TEXT("million") : TEXT("billion"); return FString::Printf(TEXT(" %s%s%s %s dollars "), *Head, *Num(N), Dec.IsEmpty() ? TEXT("") : *(TEXT(" point ") + Digits(Dec)), W); }
			return FString::Printf(TEXT(" %s%s dollar%s "), *Head, *Num(N), N == 1 ? TEXT("") : TEXT("s"));
		});
		// aircraft models A320, A350-900, 737-800
		T = Rx(T, TEXT("\\b([A-Z]{1,3})-?(\\d{2,3})(?:-(\\d{1,3}))?([A-Z]{1,2})?\\b"), [](FRegexMatcher& M)
		{ return FString::Printf(TEXT(" %s %s%s%s "), *Spaced(Grp(M, 1)), *Model(Grp(M, 2)), Grp(M, 3).IsEmpty() ? TEXT("") : *(TEXT(" ") + Model(Grp(M, 3))), Grp(M, 4).IsEmpty() ? TEXT("") : *(TEXT(" ") + Spaced(Grp(M, 4)))); });
		T = Rx(T, TEXT("\\b(\\d{3})-(\\d{3})([A-Z]{1,2})?\\b"), [](FRegexMatcher& M) { return FString::Printf(TEXT(" %s %s%s "), *Model(Grp(M, 1)), *Model(Grp(M, 2)), Grp(M, 3).IsEmpty() ? TEXT("") : *(TEXT(" ") + Spaced(Grp(M, 3)))); });
		// times 08:50
		T = Rx(T, TEXT("\\b(\\d{1,2}):(\\d{2})\\b"), [](FRegexMatcher& M)
		{ const int32 H = FCString::Atoi(*Grp(M, 1)), Mi = FCString::Atoi(*Grp(M, 2)); return FString::Printf(TEXT(" %s %s "), *Num(H), Mi == 0 ? TEXT("hundred") : Mi < 10 ? *(FString(TEXT("oh ")) + Ones[Mi]) : *Num(Mi)); });
		T = Rx(T, TEXT("(\\d)\\s?%"), [](FRegexMatcher& M) { return Grp(M, 1) + TEXT(" percent"); });
		T = Rx(T, TEXT("(\\d[\\d,.]*)\\s?(fpm|kts|kt|nm|kg|km|ft)\\b"), [](FRegexMatcher& M)
		{
			const FString U = Grp(M, 2);
			const TCHAR* W = U == TEXT("fpm") ? TEXT("feet per minute") : (U == TEXT("kt") || U == TEXT("kts")) ? TEXT("knots") : U == TEXT("nm") ? TEXT("nautical miles") : U == TEXT("kg") ? TEXT("kilograms") : U == TEXT("km") ? TEXT("kilometres") : TEXT("feet");
			return Grp(M, 1) + TEXT(" ") + W;
		});
		T = Rx(T, TEXT("\\b(\\d+)\\.(\\d+)\\b"), [](FRegexMatcher& M) { return FString::Printf(TEXT(" %s point %s "), *Num(FCString::Atoi64(*Grp(M, 1))), *Digits(Grp(M, 2))); });
		T = Rx(T, TEXT("\\b\\d{1,3}(?:,\\d{3})+\\b|\\b\\d+\\b"), [](FRegexMatcher& M) { return TEXT(" ") + Num(FCString::Atoi64(*M.GetCaptureGroup(0).Replace(TEXT(","), TEXT("")))) + TEXT(" "); });
		// registrations and codes in capitals: letter by letter
		T = Rx(T, TEXT("\\b([A-Z]{1,2})-([A-Z0-9]{2,4})\\b"), [](FRegexMatcher& M) { return TEXT(" ") + Spaced(Grp(M, 1) + Grp(M, 2)) + TEXT(" "); });
		T = Rx(T, TEXT("\\b[A-Z]{2,5}\\b"), [](FRegexMatcher& M) { const FString W = M.GetCaptureGroup(0); return (W == TEXT("OK") || W == TEXT("NATO") || W == TEXT("ATIS")) ? W : TEXT(" ") + Spaced(W) + TEXT(" "); });
		T = Rx(T, TEXT("\\s+-\\s+|\\s+—\\s+|—"), [](FRegexMatcher&) { return FString(TEXT(", ")); });
		T = T.Replace(TEXT("&"), TEXT(" and ")).Replace(TEXT("/"), TEXT(" or "));
		T = Rx(T, TEXT("\\s+([,.;:!?])"), [](FRegexMatcher& M) { return Grp(M, 1); });
		T = Rx(T, TEXT("\\s+"), [](FRegexMatcher&) { return FString(TEXT(" ")); });
		return T.TrimStartAndEnd();
	}

	// ---------------------------------------------------------------- the dictionary
	static const FString* LookP(const FString& W)
	{
		if (const FString* V = Lex.Find(W)) return V;
		const FString L = W.ToLower();
		if (L != W) if (const FString* V = Lex.Find(L)) return V;
		if (W.Len() > 1 && W == L) { FString C = W; C[0] = FChar::ToUpper(C[0]); if (const FString* V = Lex.Find(C)) return V; }
		return nullptr;
	}
	static FString Look(const FString& W) { const FString* P = LookP(W); return P ? *P : FString(); }
	static bool EndsAny(const FString& S, const TCHAR* Set) { return S.Len() && FCString::Strchr(Set, S[S.Len() - 1]) != nullptr; }
	static FString Plural(const FString& St) { return St.IsEmpty() ? St : EndsAny(St, TEXT("ptkfθ")) ? St + TEXT("s") : EndsAny(St, TEXT("szʃʒʧʤ")) ? St + TEXT("ɪz") : St + TEXT("z"); }
	/** Plurals, past tenses and -ing forms of known words. */
	static FString Stems(const FString& W)
	{
		const FString L = W.ToLower();
		if (L.EndsWith(TEXT("'s"))) { const FString P = Look(L.LeftChop(2)); if (!P.IsEmpty()) return Plural(P); }
		if (L.EndsWith(TEXT("s")) && !L.EndsWith(TEXT("ss")))
		{
			FString P = Look(L.LeftChop(1)); if (!P.IsEmpty()) return Plural(P);
			if (L.EndsWith(TEXT("ies"))) { P = Look(L.LeftChop(3) + TEXT("y")); if (!P.IsEmpty()) return Plural(P); }
			if (L.EndsWith(TEXT("es"))) { P = Look(L.LeftChop(2)); if (!P.IsEmpty()) return Plural(P); }
		}
		if (L.EndsWith(TEXT("ed")))
		{
			FString P = Look(L.LeftChop(2)); if (P.IsEmpty()) P = Look(L.LeftChop(1));
			if (!P.IsEmpty()) return EndsAny(P, TEXT("pkfθʃsʧ")) ? P + TEXT("t") : EndsAny(P, TEXT("td")) ? P + TEXT("ɪd") : P + TEXT("d");
		}
		if (L.EndsWith(TEXT("ing")) && L.Len() > 5)
		{
			FString P = Look(L.LeftChop(3)); if (P.IsEmpty()) P = Look(L.LeftChop(3) + TEXT("e"));
			if (P.IsEmpty() && L.Len() > 5 && L[L.Len() - 4] == L[L.Len() - 5]) P = Look(L.LeftChop(4));
			if (!P.IsEmpty()) return P + TEXT("ɪŋ");
		}
		if (L.EndsWith(TEXT("ly"))) { const FString P = Look(L.LeftChop(2)); if (!P.IsEmpty()) return P + TEXT("li"); }
		return FString();
	}

	// ---------------------------------------------------------------- sounding out (names, unknown words)
	static bool IsVowelSound(TCHAR C) { return FCString::Strchr(TEXT("æaɛɪɑɒʌʊiuAIOQWYɔɜə"), C) != nullptr; }
	static FString Plain(const FString& W)
	{
		// lower case, accents off (the game's NFD + strip), a few letters of their own, only a-z and '-'
		static const TCHAR* From = TEXT("àáâãäåçèéêëìíîïñòóôõöùúûüýÿšžčćśńęążź");
		static const TCHAR* To = TEXT("aaaaaaceeeeiiiinooooouuuuyyszccsneazz");
		FString O; const FString L = W.ToLower();
		for (TCHAR C : L)
		{
			if (C == 0x00F0) C = 'd'; else if (C == 0x00F8) C = 'u'; else if (C == 0x00E6) C = 'a'; else if (C == 0x0131) C = 'i'; else if (C == 0x0142) C = 'w';
			else if (const TCHAR* P = FCString::Strchr(From, C)) C = To[P - From];
			if ((C >= 'a' && C <= 'z') || C == '-') O.AppendChar(C);
		}
		return O;
	}
	static FString Sound1(TCHAR C) { for (const ADPG2PTables::FPair& R : ADPG2PTables::Rules) if (R.A[0] == C && R.A[1] == 0) return R.B; return FString(); }
	static FString SoundOut(const FString& Word)
	{
		TArray<FString> Parts; Plain(Word).ParseIntoArray(Parts, TEXT("-"), true);
		FString All;
		for (int32 Pi = 0; Pi < Parts.Num(); ++Pi)
		{
			const FString& P = Parts[Pi];
			const FString K = P.Len() > 2 ? Look(P) : FString();
			if (!K.IsEmpty() && Pi > 0) { All += K.Replace(TEXT("ˈ"), TEXT("")).Replace(TEXT("ˌ"), TEXT("")); continue; }
			if (!K.IsEmpty()) { All += K.Replace(TEXT("ˌ"), TEXT("ˈ")); continue; }
			FString Out; int32 i = 0;
			while (i < P.Len())
			{
				// magic e at the end of a syllable
				if (i + 3 == P.Len() && FCString::Strchr(TEXT("aeiou"), P[i]) && FCString::Strchr(TEXT("bcdfgklmnprstvz"), P[i + 1]) && P[i + 2] == 'e')
				{ const TCHAR V = P[i]; Out += V == 'a' ? TEXT("A") : V == 'e' ? TEXT("iː") : V == 'i' ? TEXT("I") : V == 'o' ? TEXT("Q") : TEXT("uː"); Out += Sound1(P[i + 1]); i += 3; continue; }
				// a vowel ending the syllable is long
				if (FCString::Strchr(TEXT("aeiou"), P[i]) && i == P.Len() - 1 && P.Len() > 1)
				{ const TCHAR V = P[i]; Out += V == 'a' ? TEXT("ɑː") : V == 'e' ? TEXT("A") : V == 'i' ? TEXT("iː") : V == 'o' ? TEXT("Q") : TEXT("uː"); ++i; continue; }
				if (P[i] == 'y' && i == P.Len() - 1 && i > 0) { Out += TEXT("i"); ++i; continue; }
				if (P[i] == 'y' && i == 0) { Out += TEXT("j"); ++i; continue; }
				if (P[i] == 'c' && i + 1 < P.Len() && FCString::Strchr(TEXT("eiy"), P[i + 1])) { Out += TEXT("s"); ++i; continue; }
				bool bHit = false;
				for (const ADPG2PTables::FPair& R : ADPG2PTables::Rules)
				{
					const int32 N = FCString::Strlen(R.A);
					if (i + N <= P.Len() && FCString::Strncmp(*P + i, R.A, N) == 0) { Out += R.B; i += N; bHit = true; break; }
				}
				if (!bHit) ++i;
			}
			// stress on the first vowel of the first part
			if (Pi == 0) { int32 Lead = 0; while (Lead < Out.Len() && !IsVowelSound(Out[Lead])) ++Lead; if (Lead < Out.Len()) Out = Out.Left(Lead) + TEXT("ˈ") + Out.Mid(Lead); }
			All += Out;
		}
		return All;
	}

	static FString Word(const FString& W, const FString& Next)
	{
		const FString L = W.ToLower(); const bool bNv = Next.Len() && FCString::Strchr(TEXT("aeiouAEIOU"), Next[0]);
		if (L == TEXT("the")) return bNv ? TEXT("ði") : TEXT("ðə");
		if (W == TEXT("a")) return TEXT("ɐ");
		if (L == TEXT("an")) return TEXT("ɐn");
		if (L == TEXT("to")) return bNv ? TEXT("tʊ") : TEXT("tə");
		if (L == TEXT("of")) return TEXT("əv");
		if (L == TEXT("and")) return TEXT("ənd");
		if (W.Len() == 1 && FChar::IsUpper(W[0])) { const FString P = Look(W); return P.IsEmpty() ? SoundOut(W) : P; }
		if (const FString* Nm = Names.Find(L)) if (!LookP(W)) return SoundOut(*Nm);
		FString P = Look(W); if (P.IsEmpty()) P = Stems(W);
		if (!P.IsEmpty()) return P;
		if (W.Contains(TEXT("-"))) { TArray<FString> Ps; W.ParseIntoArray(Ps, TEXT("-"), true); FString O; for (const FString& X : Ps) { if (!O.IsEmpty()) O += TEXT(" "); O += Word(X, FString()); } return O; }
		return SoundOut(W);
	}

	TArray<FString> Phonemize(const FString& Text)
	{
		TArray<FString> Out;
		if (!Ready()) return Out;
		const FString T = Normalise(Text);
		// sentences, then the words and the punctuation in each
		const FRegexPattern SP(TEXT("[^.!?]+[.!?]*")); FRegexMatcher SM(SP, T);
		while (SM.FindNext())
		{
			const FString S = SM.GetCaptureGroup(0).TrimStartAndEnd(); if (S.IsEmpty()) continue;
			TArray<FString> Toks;
			const FRegexPattern TP(TEXT("[A-Za-zÀ-ÿĀ-žḀ-ỿðÐøØæÆıłŁ'’]+(?:-[A-Za-zÀ-ÿĀ-žðøæ]+)*|[,.;:!?]"));
			FRegexMatcher TM(TP, S); while (TM.FindNext()) Toks.Add(TM.GetCaptureGroup(0));
			TArray<FString> Ps;
			for (int32 i = 0; i < Toks.Num(); ++i)
			{
				const FString& W = Toks[i];
				if (W.Len() == 1 && FCString::Strchr(TEXT(",.;:!?"), W[0])) { if (Ps.Num()) Ps.Last() += (W == TEXT(";") || W == TEXT(":")) ? TEXT(",") : *W; continue; }
				Ps.Add(Word(W, Toks.IsValidIndex(i + 1) ? Toks[i + 1] : FString()));
			}
			FString Line = FString::Join(Ps, TEXT(" "));
			while (Line.Contains(TEXT("  "))) Line = Line.Replace(TEXT("  "), TEXT(" "));
			Line.TrimStartAndEndInline();
			if (!Line.IsEmpty()) Out.Add(Line);
		}
		return Out;
	}

	TArray<TArray<int64>> Ids(const FString& Text)
	{
		TArray<TArray<int64>> Res;
		TArray<FString> Pieces;
		for (const FString& P : Phonemize(Text))
		{
			if (P.Len() <= 150) { Pieces.Add(P); continue; }
			// long ones cut at their commas
			TArray<FString> Xs; P.ParseIntoArray(Xs, TEXT(", "), true); FString Cur;
			for (int32 k = 0; k < Xs.Num(); ++k)
			{
				const FString X = Xs[k] + (k < Xs.Num() - 1 ? TEXT(",") : TEXT(""));
				if (!Cur.IsEmpty() && Cur.Len() + X.Len() > 150) { Pieces.Add(Cur); Cur = X; } else Cur = Cur.IsEmpty() ? X : Cur + TEXT(" ") + X;
			}
			if (!Cur.IsEmpty()) Pieces.Add(Cur);
		}
		for (const FString& P : Pieces)
		{
			TArray<int64> A; for (const TCHAR C : P) if (const int64* I = IdOf.Find(C)) A.Add(*I);
			if (A.Num()) Res.Add(MoveTemp(A));
		}
		return Res;
	}
}
