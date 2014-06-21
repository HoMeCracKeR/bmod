/**
 * References:
 *   http://www.read.seas.harvard.edu/~kohler/class/04f-aos/ref/i386.pdf
 *   http://ref.x86asm.net
 */

#include <QDebug>
#include <QBuffer>

#include "AsmX86.h"
#include "../Util.h"
#include "../Reader.h"
#include "../Section.h"

AsmX86::AsmX86(BinaryObjectPtr obj) : obj{obj}, reader{nullptr} { }

bool AsmX86::disassemble(SectionPtr sec, Disassembly &result) {
  QBuffer buf;
  buf.setData(sec->getData());
  buf.open(QIODevice::ReadOnly);
  reader.reset(new Reader(buf));

  // Address of main()
  quint64 funcAddr = sec->getAddress();

  bool ok{true}, peek{false};
  unsigned char ch, nch, ch2, r, mod, op1, op2;
  quint32 num{0};
  qint64 pos{0};
  while (!reader->atEnd()) {
    // Handle special NOP sequences.
    if (handleNops(result)) {
      continue;
    }

    // Handle other instructions.
    pos = reader->pos();
    ch = reader->getUChar(&ok);
    if (!ok) return false;

    nch = reader->peekUChar(&peek);

    // ADD (r/m16/32  r16/32)
    if (ch == 0x01 && peek) {
      reader->getUChar(); // eat
      addResult("addl " + getModRMByte(nch, RegType::R32, RegType::R32),
                pos, result);
    }

    // ADD (r16/32  r/m16/32)
    else if (ch == 0x03 && peek) {
      reader->getUChar(); // eat
      addResult("addl " + getModRMByte(nch, RegType::R32, RegType::R32),
                pos, result);
    }

    // AND (eAX  imm16/32)
    else if (ch == 0x25) {
      num = reader->getUInt32(&ok);
      if (!ok) return false;
      addResult("addl $" + formatHex(num, 8) + ",%eax", pos, result);
    }

    // CMP (r16/32 r/m16/32) (reverse)
    else if (ch == 0x3B && peek) {
      reader->getUChar(); // eat
      addResult("cmpl " + getModRMByte(nch, RegType::R32, RegType::R32, true),
                pos, result);
    }

    // CMP (eAX  imm16/32)
    else if (ch == 0x3D) {
      num = reader->getUInt32(&ok);
      if (!ok) return false;
      addResult("cmpl $" + formatHex(num, 8) + ",%eax", pos, result);
    }

    // PUSH
    else if (ch >= 0x50 && ch <= 0x57) {
      r = ch - 0x50;
      addResult("pushl " + getReg(RegType::R32, r), pos, result);
    }

    // POP
    else if (ch >= 0x58 && ch <= 0x60) {
      r = ch - 0x58;
      addResult("popl " + getReg(RegType::R32, r), pos, result);
    }

    // ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
    // (r/m16/32	imm16/32)
    else if (ch == 0x81 && peek) {
      reader->getUChar(); // eat
      splitByteModRM(nch, mod, op1, op2);

      num = reader->getUInt32(&ok);
      if (!ok) return false;

      QString inst;
      if (op1 == 0) {
        inst = "addl";
      }
      else if (op1 == 1) {
        inst = "orl";
      }
      else if (op1 == 2) {
        inst = "adcl"; // Add with carry
      }
      else if (op1 == 3) {
        inst = "sbbl"; // Integer subtraction with borrow
      }
      else if (op1 == 4) {
        inst = "andl";
      }
      else if (op1 == 5) {
        inst = "subl";
      }
      else if (op1 == 6) {
        inst = "xorl";
      }
      else if (op1 == 7) {
        inst = "cmpl";
      }

      addResult(inst + " $" + formatHex(num, 8) + "," +
                getReg(RegType::R32, op2), pos, result);
    }

    // ADD, OR, ADC, SBB, AND, SUB, XOR, CMP
    // (r/m16/32	imm8)
    else if (ch == 0x83 && peek) {
      reader->getUChar(); // eat
      splitByteModRM(nch, mod, op1, op2);

      ch2 = reader->getUChar(&ok);
      if (!ok) return false;

      QString inst;
      if (op1 == 0) {
        inst = "addl";
      }
      else if (op1 == 1) {
        inst = "orl";
      }
      else if (op1 == 2) {
        inst = "adcl"; // Add with carry
      }
      else if (op1 == 3) {
        inst = "sbbl"; // Integer subtraction with borrow
      }
      else if (op1 == 4) {
        inst = "andl";
      }
      else if (op1 == 5) {
        inst = "subl";
      }
      else if (op1 == 6) {
        inst = "xorl";
      }
      else if (op1 == 7) {
        inst = "cmpl";
      }

      addResult(inst + " $" + formatHex(ch2, 2) + "," +
                getReg(RegType::R32, op2), pos, result);
    }

    // MOV (r/m8	r8)
    else if (ch == 0x88 && peek) {
      reader->getUChar(); // eat
      addResult("movb " + getModRMByte(nch, RegType::R8, RegType::R32),
                pos, result);
    }

    // MOV (r/m16/32	r16/32)
    else if (ch == 0x89 && peek) {
      reader->getUChar(); // eat
      addResult("movl " + getModRMByte(nch, RegType::R32, RegType::R32),
                pos, result);
    }

    // MOV (r8  r/m8) (reverse of 0x88)
    else if (ch == 0x8A && peek) {
      reader->getUChar(); // eat
      addResult("movb " + getModRMByte(nch, RegType::R8, RegType::R32, true),
                pos, result);
    }

    // MOV (r16/32	r/m16/32) (reverse of 0x89)
    else if (ch == 0x8B) {
      reader->getUChar(); // eat
      addResult("movl " + getModRMByte(nch, RegType::R32, RegType::R32, true),
                pos, result);
    }

    // LEA (r16/32	m) Load Effective Address
    else if (ch == 0x8D && peek) {
      reader->getUChar(); // eat
      if (!ok) return false;
      addResult("leal " + getModRMByte(nch, RegType::R32, RegType::R32, true),
                pos, result);
    }

    // NOP
    else if (ch == 0x90) {
      addResult("nop", pos, result);
    }

    // TEST	(AL	imm8)
    else if (ch == 0xA8 && peek) {
      reader->getUChar(); // eat
      addResult("testb $" + formatHex(nch, 2) + ",%al", pos, result);
    }

    // MOV (r16/32	imm16/32)
    else if (ch >= 0xB8 && ch <= 0xBF) {
      splitByteModRM(ch, mod, op1, op2);
      num = reader->getUInt32(&ok);
      if (!ok) return false;
      addResult("movl $" + formatHex(num, 8) + "," +
                getModRMByte(op2, RegType::R32, RegType::R32), pos, result);
    }

    // RETN
    else if (ch == 0xC3) {
      addResult("ret", pos, result);
    }

    // MOV (r/m8  imm8)
    else if (ch == 0xC6 && peek) {
      reader->getUChar(); // eat
      if (!ok) return false;
      addResult("movb " + getModRMByte(nch, RegType::R8, RegType::R32, false, 0),
                pos, result);
    }

    // MOV (r/m16/32	imm16/32)
    else if (ch == 0xC7 && peek) {
      reader->getUChar(); // eat
      splitByteModRM(nch, mod, op1, op2);
      ch2 = reader->getUChar(&ok);
      if (!ok) return false;
      num = reader->getUInt32(&ok);
      if (!ok) return false;
      addResult("movl $" + formatHex(num, 8) + "," + formatHex(ch2, 2) + "(" +
                getReg(RegType::R32, op2) + ")", pos, result);
    }

    // Call (relative function address)
    else if (ch == 0xE8) {
      num = reader->getUInt32(&ok);
      if (!ok) return false;
      addResult("calll " + formatHex(funcAddr + reader->pos() + num, 8),
                pos, result);
    }

    // JMP (rel16/32) (relative address)
    else if (ch == 0xE9) {
      num = reader->getUInt32(&ok);
      if (!ok) return false;
      addResult("jmp " + formatHex(funcAddr + reader->pos() + num, 8),
                pos, result);
    }

    // INC, DEC, CALL, CALLF, JMP, JMPF, PUSH
    else if (ch == 0xFF && peek) {
      reader->getUChar(); // eat
      splitByteModRM(nch, mod, op1, op2);

      QString inst;
      if (op1 == 0) {
        inst = "inc ";
      }
      else if (op1 == 1) {
        inst = "dec ";
      }
      else if (op1 == 2) {
        inst = "call *";
      }
      else if (op1 == 3) {
        inst = "callf ";
      }
      else if (op1 == 4) {
        inst = "jmp ";
      }
      else if (op1 == 5) {
        inst = "jmpf ";
      }
      else if (op1 == 6) {
        inst = "pushl ";
      }

      addResult(inst + getReg(RegType::R32, op2), pos, result);
    }

    // Two-byte instructions.
    else if (ch == 0x0F && peek) {
      ch = nch;
      reader->getUChar(); // eat

      nch = reader->peekUChar(&peek);

      // NOP (r/m16/32)
      if (ch == 0x1F && peek) {
        reader->getUChar(); // eat
        splitByteModRM(nch, mod, op1, op2);
        num = reader->getUInt32(&ok);
        if (!ok) return false;
        addResult("nopl " + formatHex(num, 8) +
                  "(" + getReg(RegType::R32, op1) + ")",
                  pos, result);
      }

      // JNZ (rel16/32) or JNE (rel16/32), same functionality
      // different name. Relative function address.
      else if (ch == 0x85) {
        num = reader->getUInt32(&ok);
        if (!ok) return false;
        addResult("jne " + formatHex(funcAddr + reader->pos() + num, 8),
                  pos, result);
      }

      // MOVSX (r16/32 r/m8) (reverse)
      // Move with Sign-Extension
      else if (ch == 0xBE && peek) {
        reader->getUChar(); // eat
        if (!ok) return false;
        addResult("movsbl " + getModRMByte(nch, RegType::R32, RegType::R8, true),
                  pos, result);
      }
    }

    // Unsupported
    else {
      addResult("Unsupported: " + QString::number(ch, 16).toUpper(),
                pos, result);
    }
  }

  return !result.asmLines.isEmpty();
}

bool AsmX86::handleNops(Disassembly &result) {
  qint64 pos = reader->pos();
  if (reader->peekList({0x66, 0x90})) {
    reader->read(2);
    addResult("xchg %ax,%ax", pos, result);
    return true;
  }
  else if (reader->peekList({0x0f, 0x1f, 0x00})) {
    reader->read(3);
    addResult("nopl 0x0(%eax)", pos, result);
    return true;
  }
  else if (reader->peekList({0x0f, 0x1f, 0x44, 0x00, 0x00})) {
    reader->read(5);
    addResult("nopl 0x0(%eax,%eax)", pos, result);
    return true;
  }
  else if (reader->peekList({0x66, 0x0f, 0x1f, 0x44, 0x00, 0x00})) {
    reader->read(6);
    addResult("nopw 0x0(%eax,%eax)", pos, result);
    return true;
  }
  else if (reader->peekList({0x0f, 0x1f, 0x80, 0x00, 0x00, 0x00, 0x00})) {
    reader->read(7);
    addResult("nopl 0L(%eax)", pos, result);
    return true;
  }
  else if (reader->peekList({0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00})) {
    reader->read(8);
    addResult("nopl 0L(%eax,%eax)", pos, result);
    return true;
  }
  else if (reader->peekList({0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00})) {
    reader->read(9);
    addResult("nopw 0L(%eax,%eax)", pos, result);
    return true;
  }
  else if (reader->peekList({0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00})) {
    reader->read(10);
    addResult("nopw %cs:0L(%eax,%eax)", pos, result);
    return true;
  }
  return false;
}

void AsmX86::addResult(const QString &line, qint64 pos, Disassembly &result) {
  result.asmLines << line;
  result.bytesConsumed << reader->pos() - pos;
}

void AsmX86::splitByteModRM(unsigned char num, unsigned char &mod,
                            unsigned char &op1, unsigned char &op2) {
  mod = (num & 0xC0) >> 6; // 2 first bits
  op1 = (num & 0x38) >> 3; // 3 next bits
  op2 = num & 0x7; // 3 last bits  
}

QString AsmX86::getReg(RegType type, int num) {
  if (num < 0 || num > 7) {
    return QString();
  }
  static QString regs[6][8] = {{"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"},
                               {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"},
                               {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"},
                               {"mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"},
                               {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"},
                               {"es", "cs", "ss", "ds", "fs", "gs", "reserved", "reserved"}};
  return "%" + regs[type][num];
}

QString AsmX86::getModRMByte(unsigned char num, RegType type1, RegType type2,
                             bool swap, int imm) {
  unsigned char mod, op1, op2;
  splitByteModRM(num, mod, op1, op2);

  // [reg]
  if (mod == 0) {
    if (op1 != 4 && op1 != 5 && op2 != 4 && op2 != 5) {
      return (!swap
              ? getReg(type1, op1) + ",(" + getReg(type2, op2) + ")"
              : "(" + getReg(type2, op2) + ")," + getReg(type1, op1));
    }
    else if (op1 == 4) {
      unsigned char num = reader->getUChar();
      unsigned char m, o1, o2;
      splitByteModRM(num, m, o1, o2);
      return getReg(type1, op2) + ",(" + getReg(type2, o2) + ")";
    }
    else if (op2 == 4) {
      unsigned char num = reader->getUChar();
      unsigned char m, o1, o2;
      splitByteModRM(num, m, o1, o2);
      return getReg(type1, op1) + ",(" + getReg(type2, o1) + ")";
    }
    else {
      qDebug() << "unsupported mod=0 op1/op2 = 5" << op1 << op2;
      qDebug() << "this: " << QString::number(num, 16);
    }
  }

  // [reg]+disp8
  else if (mod == 1) {
    unsigned char num = reader->getUChar();
    QString imms;
    if (imm != -1) {
      unsigned char num2 = reader->getUChar();
      imms = "$" + formatHex(num2, 2);
    }
    return (!swap
            ? (imm == 0 ? imms : getReg(type1, op1)) + "," +
              formatHex(num, 2) + "(" + getReg(type2, op2) + ")"
            : formatHex(num, 2) + "(" + getReg(type2, op2) + ")," +
              (imm == 0 ? imms : getReg(type1, op1)));
  }

  // [reg]+disp32
  else if (mod == 2) {
    quint32 num = reader->getUInt32();
    return (!swap
            ? getReg(type1, op1) + "," + formatHex(num, 8) + "(" +
            getReg(type2, op2) + ")"
            : formatHex(num, 8) + "(" + getReg(type2, op2) + ")," +
            getReg(type1, op1));
  }

  else if (mod == 3) {
    return (!swap
            ? getReg(type1, op1) + "," + getReg(type2, op2)
            : getReg(type2, op2) + "," + getReg(type1, op1));
  }

  return QString();
}

QString AsmX86::formatHex(quint32 num, int len) {
  return "0x" + Util::padString(QString::number(num, 16), len);
}
