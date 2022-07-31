#include "pipeline_symbol_collection32.h"

#include "tools_elf.h"
#include "symbol.h"

static enum symbol_type determinate_symbol_type32(Elf32_Ehdr *elf, Elf32_Sym *raw_symbol)
{
	int binding = ELF32_ST_BIND(raw_symbol->st_info);
	int type = ELF32_ST_TYPE(raw_symbol->st_info);

	size_t section_index = raw_symbol->st_shndx;
	Elf32_Shdr *section = elf32_get_section(elf, section_index);
	Elf32_Word section_type = section->sh_type;

	// Analyze symbol type.

	if (type == STT_GNU_IFUNC)
		return symbol_type_indirect;

	// Analyze symbol binding.

	if (binding == STB_GNU_UNIQUE)
		return symbol_type_unique;
	if (binding == STB_WEAK)
	{
		if (type == STT_OBJECT)
			return symbol_type_weak_object + (section_index != SHN_UNDEF);
		else
			return symbol_type_weak + (section_index != SHN_UNDEF);
	}

	// Analyze section type.

	if (section_type == SHT_NOBITS)
		return symbol_type_uninitialized_data;

	// Analyze section index.

	if (section_index == SHN_ABS)
		return symbol_type_absolute;
	if (section_index == SHN_COMMON)
		return symbol_type_common;
	if (section_index == SHN_UNDEF)
		return symbol_type_undefined;

	// Analyze section flags.

	if ((section->sh_flags & (SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE)) == SHF_ALLOC)
		return symbol_type_read_only_data;
	if ((section->sh_flags & (SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE)) == (SHF_ALLOC | SHF_WRITE))
		return symbol_type_initialized_data;
	if ((section->sh_flags & (SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE)) == (SHF_ALLOC | SHF_EXECINSTR))
		return symbol_type_code;

	return symbol_type_unknown;
}

enum symbol_binding determine_symbol_binding32(Elf32_Ehdr *elf, Elf32_Sym *raw_symbol)
{
	switch (ELF32_ST_BIND(raw_symbol->st_info))
	{
		case STB_LOCAL:
			return symbol_binding_local;

		case STB_GLOBAL:
			return symbol_binding_global;

		case STB_WEAK:
			return symbol_binding_weak;

		case STB_GNU_UNIQUE:
			return symbol_binding_unique;

		default:
			return symbol_binding_undefined;
	}
}

void collect_symbols_in_section32(Elf32_Ehdr *elf, Elf32_Shdr *section, t_vector *output)
{
	// Resolve elf symbols.
	Elf32_Sym *raw_symbols = elf32_get_ptr(elf, section->sh_offset);
	size_t raw_symbols_count = section->sh_size / section->sh_entsize;

	// Resolve string table.
	Elf32_Shdr *string_table_section = elf32_get_section(elf, section->sh_link);
	char *string_table = elf32_get_ptr(elf, string_table_section->sh_offset);

	// Collect symbols.
	for (int i = 0; i < raw_symbols_count; i++)
	{
		Elf32_Sym *raw_symbol = raw_symbols + i;

		if (raw_symbol->st_name == 0)
			continue;

		int raw_type = ELF32_ST_TYPE(raw_symbol->st_info);
		if (raw_type == STT_FILE || raw_type == STT_SECTION)
			continue;

		struct symbol symbol;
		symbol.name = string_table + raw_symbol->st_name;
		symbol.value = raw_symbol->st_value;
		symbol.type = determinate_symbol_type32(elf, raw_symbol);
		symbol.binding = determine_symbol_binding32(elf, raw_symbol);
		symbol.is_section_undefined = raw_symbol->st_shndx == SHN_UNDEF;

		vector_push(output, &symbol);
	}
}

void pipeline_collect_symbols32(struct pipeline_context *context)
{
	Elf32_Ehdr *elf = (Elf32_Ehdr *)context->file.ptr;

	for (int i = 0; i < elf->e_shnum; i++)
	{
		Elf32_Shdr *section = elf32_get_section(elf, i);
		if (section->sh_type == SHT_SYMTAB)
			collect_symbols_in_section32(elf, section, context->symbols);
	}
}