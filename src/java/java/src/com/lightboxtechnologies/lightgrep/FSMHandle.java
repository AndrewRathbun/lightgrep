package com.lightboxtechnologies.lightgrep;

import static com.lightboxtechnologies.lightgrep.Throws.*;

public class FSMHandle extends Handle {
  static {
    LibraryLoader.init();
  }

  /**
   * @throws IndexOutOfBoundsException
   */
  public FSMHandle(int patternCountHint, int numFsmStateSizeHint) {
    super(0);
    throwIfNegative("patternCountHint", patternCountHint);
    throwIfNegative("numFsmStateSizeHint", numFsmStateSizeHint);
    Pointer = FSMHandle.create(patternCountHint, numFsmStateSizeHint);
  }

  private static native long create(int patternCountHint, int numFsmStateSizeHint);

  /**
   * Releases resources held by the {@code FSMHandle}. {@code destroy()}
   * is idempotent. Other methods <b>MUST NOT</b> be called on a
   * {@code FSMHandle} which has been destroyed.
   */
  public native void destroy();

  /**
   * @throws IllegalStateException
   * @throws NullPointerException
   *
   * FIXME: some unknown encoding exception
   */
  public int addPattern(PatternHandle hPattern, String encoding, int userIndex) {
    throwIfNull("hPattern", hPattern);
    throwIfNull("encoding", encoding);
    throwIfDestroyed(this);
    throwIfDestroyed(hPattern);
    return addPatternImpl(hPattern, encoding, userIndex);
  }

  private native int addPatternImpl(PatternHandle hPattern, String encoding, int userIndex);

  /**
   * @throws IllegalStateException
   */
  public int count() {
    throwIfDestroyed(this);
    return countImpl();
  }

  private native int countImpl();
}
